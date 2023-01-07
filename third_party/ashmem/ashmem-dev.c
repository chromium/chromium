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
#include <fcntl.h>

#include <linux/ashmem.h>
#include <sys/system_properties.h>

#define ASHMEM_DEVICE  "/dev/ashmem"

/* Technical note regarding reading system properties.
 *
 * Try to use the new __system_property_read_callback API that appeared in
 * Android O / API level 26 when available. Otherwise use the deprecated
 * __system_property_get function.
 *
 * For more technical details from an NDK maintainer, see:
 * https://bugs.chromium.org/p/chromium/issues/detail?id=392191#c17
 */

/* Weak symbol import */
void __system_property_read_callback(
    const prop_info* info,
    void (*callback)(
        void* cookie, const char* name, const char* value, uint32_t serial),
    void* cookie) __attribute__((weak));

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
  if (__system_property_read_callback) {
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
static int ashmem_dev_fd_check(int fd) {
  if (device_api_level() <= __ANDROID_API_O_MR1__)
    return 1;
  if (ashmem_get_status() == ASHMEM_STATUS_SUPPORTED) {
    struct stat st;
    return (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode) &&
            st.st_dev != 0 && st.st_dev == s_ashmem_dev);
  }
  return 0;
}

/*
 * ashmem_create_region - creates a new ashmem region and returns the file
 * descriptor, or <0 on error
 *
 * `name' is an optional label to give the region (visible in /proc/pid/maps)
 * `size' is the size of the region, in page-aligned bytes
 */
static int ashmem_dev_create_region(const char *name, size_t size) {
  int fd = open(ASHMEM_DEVICE, O_RDWR);
  if (fd < 0)
    return fd;

  int ret;
  if (name) {
    char buf[ASHMEM_NAME_LEN];
    strlcpy(buf, name, sizeof(buf));
    ret = ioctl(fd, ASHMEM_SET_NAME, buf);
    if (ret < 0)
      goto error;
  }
  ret = ioctl(fd, ASHMEM_SET_SIZE, size);
  if (ret < 0)
    goto error;

  return fd;

error:
  close(fd);
  return ret;
}

static int ashmem_dev_set_prot_region(int fd, int prot) {
  return ioctl(fd, ASHMEM_SET_PROT_MASK, prot);
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
// libandroid.so should be used to create shared memory regions.
typedef int(*ASharedMemory_createFunc)(const char*, size_t);
typedef size_t(*ASharedMemory_getSizeFunc)(int fd);
typedef int(*ASharedMemory_setProtFunc)(int fd, int prot);

// Function pointers to shared memory functions.
typedef struct {
  ASharedMemory_createFunc create;
  ASharedMemory_getSizeFunc getSize;
  ASharedMemory_setProtFunc setProt;
} ASharedMemoryFuncs;

static ASharedMemoryFuncs s_ashmem_funcs = {};
static pthread_once_t s_ashmem_funcs_once = PTHREAD_ONCE_INIT;

static void ashmem_init_funcs() {
  ASharedMemoryFuncs* funcs = &s_ashmem_funcs;
  if (device_api_level() >= __ANDROID_API_O__) {
    /* Leaked intentionally! */
    void* lib = dlopen("libandroid.so", RTLD_NOW);
    funcs->create =
        (ASharedMemory_createFunc)dlsym(lib, "ASharedMemory_create");
    funcs->getSize =
        (ASharedMemory_getSizeFunc)dlsym(lib, "ASharedMemory_getSize");
    funcs->setProt =
        (ASharedMemory_setProtFunc)dlsym(lib, "ASharedMemory_setProt");
  } else {
    funcs->create = &ashmem_dev_create_region;
    funcs->getSize = &ashmem_dev_get_size_region;
    funcs->setProt = &ashmem_dev_set_prot_region;
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

int ashmem_get_prot_region(int fd) {
  if (ashmem_dev_fd_check(fd))
    return ashmem_dev_get_prot_region(fd);
  /* There are only two practical values to return here: either
   * PROT_READ|PROT_WRITE or just PROT_READ, so try to determine
   * the flags by trying to mmap() the region read-write first.
   */
  int result = PROT_READ;
  const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  void* m = mmap(NULL, page_size, PROT_READ|PROT_WRITE,
                 MAP_SHARED, fd, 0);
  if (m != MAP_FAILED) {
    munmap(m, page_size);
    result = PROT_READ|PROT_WRITE;
  }
  return result;
}

int ashmem_pin_region(int fd, size_t offset, size_t len) {
 if (ashmem_dev_fd_check(fd))
   return ashmem_dev_pin_region(fd, offset, len);
  return ASHMEM_NOT_PURGED;
}

int ashmem_unpin_region(int fd, size_t offset, size_t len) {
  if (ashmem_dev_fd_check(fd))
    return ashmem_dev_unpin_region(fd, offset, len);
  /* NOTE: It is not possible to use madvise() here because it requires a
   * memory address. This could be done in the caller though, instead of
   * this function. */
  return 0;
}

int ashmem_get_size_region(int fd) {
  /* NOTE: Original API returns an int. Avoid breaking it. */
  return (int)ashmem_get_funcs()->getSize(fd);
}

int ashmem_device_is_supported(void) {
  return ashmem_get_status() == ASHMEM_STATUS_SUPPORTED;
}
