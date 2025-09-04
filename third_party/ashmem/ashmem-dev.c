/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ashmem.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/stat.h>  /* for fdstat() */
#include <sys/syscall.h>
#include <fcntl.h>

#include <android/log.h>
#include <linux/ashmem.h>
#include <sys/system_properties.h>

#define ASHMEM_DEVICE  "/dev/ashmem"
#define LOG_E(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "chromium-ashmem", __VA_ARGS__))

/* Technical note regarding reading system properties.
 *
 * Try to use the new __system_property_read_callback API that appeared in
 * Android O / API level 26 when available. Otherwise use the deprecated
 * __system_property_get function.
 *
 * For more technical details from an NDK maintainer, see:
 * https://bugs.chromium.org/p/chromium/issues/detail?id=392191#c17
 */

/* Callback used with __system_property_read_callback. */
static void prop_read_int(void* cookie,
                          const char* name,
                          const char* value,
                          uint32_t serial) {
  *(int *)cookie = atoi(value);
  (void)name;
  (void)serial;
}

static int system_property_get_int(const char* name) {
  int result = 0;
  if (__builtin_available(android 26, *)) {
    const prop_info* info = __system_property_find(name);
    if (info)
      __system_property_read_callback(info, &prop_read_int, &result);
  } else {
    char value[PROP_VALUE_MAX] = {};
    if (__system_property_get(name, value) >= 1)
      result = atoi(value);
  }
  return result;
}

static int device_api_level() {
  static int s_api_level = -1;
  if (s_api_level < 0)
    s_api_level = system_property_get_int("ro.build.version.sdk");
  return s_api_level;
}

static int vendor_api_level() {
  static int v_api_level = -1;
  if (v_api_level < 0)
    v_api_level = system_property_get_int("ro.vendor.api_level");
  return v_api_level;
}

typedef enum {
  ASHMEM_STATUS_INIT,
  ASHMEM_STATUS_NOT_SUPPORTED,
  ASHMEM_STATUS_SUPPORTED,
} AshmemStatus;

static AshmemStatus s_ashmem_status = ASHMEM_STATUS_INIT;
static dev_t s_ashmem_dev;

/* Return the dev_t of a given file path, or 0 if not available, */
static dev_t ashmem_find_dev(const char* path) {
  struct stat st;
  dev_t result = 0;
  if (stat(path, &st) == 0 && S_ISCHR(st.st_mode))
    result = st.st_dev;
  return result;
}

static AshmemStatus ashmem_get_status(void) {
  /* NOTE: No need to make this thread-safe, assuming that
   * all threads will find the same value. */
  if (s_ashmem_status != ASHMEM_STATUS_INIT)
    return s_ashmem_status;

  s_ashmem_dev = ashmem_find_dev(ASHMEM_DEVICE);
  s_ashmem_status = (s_ashmem_dev == 0) ? ASHMEM_STATUS_NOT_SUPPORTED
                                        : ASHMEM_STATUS_SUPPORTED;
  return s_ashmem_status;
}

/* Returns true iff the ashmem device ioctl should be used for a given fd.
 * NOTE: Try not to use fstat() when possible to avoid performance issues. */
static int is_ashmem_fd(int fd) {
  if (device_api_level() <= __ANDROID_API_O_MR1__)
    return 1;
  if (ashmem_get_status() == ASHMEM_STATUS_SUPPORTED) {
    struct stat st;
    return (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode) &&
            st.st_dev != 0 && st.st_dev == s_ashmem_dev);
  }
  return 0;
}

static int ashmem_dev_get_prot_region(int fd) {
  return ioctl(fd, ASHMEM_GET_PROT_MASK);
}

static int ashmem_dev_pin_region(int fd, size_t offset, size_t len) {
  struct ashmem_pin pin = { offset, len };
  return ioctl(fd, ASHMEM_PIN, &pin);
}

static int ashmem_dev_unpin_region(int fd, size_t offset, size_t len) {
  struct ashmem_pin pin = { offset, len };
  return ioctl(fd, ASHMEM_UNPIN, &pin);
}

static size_t ashmem_dev_get_size_region(int fd) {
  return ioctl(fd, ASHMEM_GET_SIZE, NULL);
}

// Starting with API level 26, the following functions from
// libandroid.so should be used to create shared memory regions,
// unless the device's vendor.api_level is 202604 (Android 17)
// or newer, in which case, use memfd directly instead of
// the ASharedMemory API.
typedef int(*ASharedMemory_createFunc)(const char*, size_t);
// ASharedMemory_setProtFunc() is typically invoked in conjunction with ASharedMemory_createFunc(),
// so it's okay for setProt to implicitly assume the type of fd it needs to work with.
typedef int(*ASharedMemory_setProtFunc)(int fd, int prot);

// Function pointers to shared memory functions.
typedef struct {
  ASharedMemory_createFunc create;
  ASharedMemory_setProtFunc setProt;
} ASharedMemoryFuncs;

static ASharedMemoryFuncs s_ashmem_funcs = {};
static pthread_once_t s_ashmem_funcs_once = PTHREAD_ONCE_INIT;

static int memfd_create_region(const char *name, size_t size) {
  int fd = syscall(__NR_memfd_create, name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) {
    LOG_E("memfd_create(%s, %zd) failed: %m", name, size);
    return fd;
  }

  int ret = ftruncate(fd, size);
  if (ret < 0) {
    LOG_E("ftruncate(%s, %zd) failed: %m", name, size);
    goto error;
  }

  ret = fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK);
  if (ret < 0) {
    LOG_E("memfd_create(%s, %zd) fcntl(F_ADD_SEALS) failed: %m", name, size);
    goto error;
  }

  return fd;

error:
  close(fd);
  return ret;
}

static int memfd_set_prot_region(int fd, int prot) {
  int seals = fcntl(fd, F_GET_SEALS);
  if (seals == -1) {
    LOG_E("memfd_set_prot_region(%d, %d): F_GET_SEALS failed: %m", fd, prot);
    return -1;
  }

  if (prot & PROT_WRITE) {
    /*
     * Now we want the buffer to be read-write, let's check if the buffer
     * has been previously marked as read-only before, if so return error
     */
    if (seals & F_SEAL_FUTURE_WRITE) {
      LOG_E("memfd_set_prot_region(%d, %d): region is write protected", fd, prot);
      // Inline with ashmem error code, if already in read-only mode.
      errno = EINVAL;
      return -1;
    }

    return 0;
  }

  // We would only allow read-only for any future file operations
  if (fcntl(fd, F_ADD_SEALS, F_SEAL_FUTURE_WRITE) == -1) {
    LOG_E("memfd_set_prot_region(%d, %d): F_SEAL_FUTURE_WRITE seal failed: %m", fd, prot);
    return -1;
  }

  return 0;
}

static int memfd_get_prot_region(int fd) {
  int prot = PROT_READ;
  int seals = fcntl(fd, F_GET_SEALS);
  if (seals == -1)
    LOG_E("memfd_get_prot_region(%d): F_GET_SEALS failed: %m", fd);
  else if (!(seals & (F_SEAL_FUTURE_WRITE | F_SEAL_WRITE)))
    prot |= PROT_WRITE;
  return prot;
}

static void ashmem_init_funcs() {
  ASharedMemoryFuncs* funcs = &s_ashmem_funcs;
  /*
   * When a device conforms to the VSR for API level 202604 (Android 17),
   * ASharedMemory will allocate memfds and attempt to relabel them by using
   * fsetxattr() to workaround how SELinux handles memfds.
   *
   * fsetxattr() is not allowlisted in our seccomp filter, and allowlisting
   * it may be unsafe. Since memfds from Chromium should be accessible with
   * the existing sepolicy for appdomain_tmpfs files, just allocate memfds
   * directly if the device conforms to the VSR for API level 202604.
   */
  if (vendor_api_level() >= 202604) {
    funcs->create = &memfd_create_region;
    funcs->setProt = &memfd_set_prot_region;
  } else {
    /* Leaked intentionally! */
    void* lib = dlopen("libandroid.so", RTLD_NOW);
    funcs->create =
        (ASharedMemory_createFunc)dlsym(lib, "ASharedMemory_create");
    funcs->setProt =
        (ASharedMemory_setProtFunc)dlsym(lib, "ASharedMemory_setProt");
  }
}

static const ASharedMemoryFuncs* ashmem_get_funcs() {
  pthread_once(&s_ashmem_funcs_once, ashmem_init_funcs);
  return &s_ashmem_funcs;
}

int ashmem_create_region(const char* name, size_t size) {
  return ashmem_get_funcs()->create(name, size);
}

int ashmem_set_prot_region(int fd, int prot) {
  return ashmem_get_funcs()->setProt(fd, prot);
}

static bool is_memfd_fd(int fd) {
  if (fcntl(fd, F_GET_SEALS, 0) == -1)
    return false;
  return true;
}

int ashmem_get_prot_region(int fd) {
  if (is_memfd_fd(fd))
    return memfd_get_prot_region(fd);

  if (is_ashmem_fd(fd))
    return ashmem_dev_get_prot_region(fd);

  return -1;
}

int ashmem_pin_region(int fd, size_t offset, size_t len) {
 if (is_ashmem_fd(fd))
   return ashmem_dev_pin_region(fd, offset, len);
  return ASHMEM_NOT_PURGED;
}

int ashmem_unpin_region(int fd, size_t offset, size_t len) {
  if (is_ashmem_fd(fd))
    return ashmem_dev_unpin_region(fd, offset, len);
  /* NOTE: It is not possible to use madvise() here because it requires a
   * memory address. This could be done in the caller though, instead of
   * this function. */
  return 0;
}

int ashmem_get_size_region(int fd) {
  if (is_ashmem_fd(fd))
    return ashmem_dev_get_size_region(fd);

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    LOG_E("fstat(%d) failed: %m", fd);
    return -1;
  }

  return sb.st_size;
}

int ashmem_device_is_supported(void) {
  return ashmem_get_status() == ASHMEM_STATUS_SUPPORTED;
}
