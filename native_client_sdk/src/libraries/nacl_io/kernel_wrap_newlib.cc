// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>  // Include something that will define __GLIBC__.
#include "nacl_io/kernel_wrap.h" // IRT_EXT is turned on in this header.

// The entire file is wrapped in this #if. We do this so this .cc file can be
// compiled, even on a non-newlib build.
#if !defined(NACL_IO_IRT_EXT) && defined(__native_client__) && \
    !defined(__GLIBC__) && !defined(__BIONIC__)

#include <dirent.h>
#include <errno.h>
#include <irt.h>
#include <irt_dev.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/log.h"

EXTERN_C_BEGIN

// Macro to get the REAL function pointer
#define REAL(name) __nacl_irt_##name##_real

// Macro to get the WRAP function
#define WRAP(name) __nacl_irt_##name##_wrap

// Declare REAL function pointer.
#define DECLARE_REAL_PTR(group, name) \
  __typeof__(__libnacl_irt_##group.name) REAL(name);

// Assign the REAL function pointer.
#define ASSIGN_REAL_PTR(group, name) REAL(name) = __libnacl_irt_##group.name;

// Switch IRT's pointer to the REAL pointer
#define USE_REAL(group, name) \
  __libnacl_irt_##group.name = (__typeof__(REAL(name)))REAL(name);

// Switch the IRT's pointer to the WRAP function
#define USE_WRAP(group, name) \
  __libnacl_irt_##group.name = (__typeof__(REAL(name)))WRAP(name);

extern void __libnacl_irt_dev_filename_init(void);
extern void __libnacl_irt_dev_fdio_init(void);

extern struct nacl_irt_basic __libnacl_irt_basic;
extern struct nacl_irt_fdio __libnacl_irt_fdio;
extern struct nacl_irt_dev_fdio __libnacl_irt_dev_fdio;
extern struct nacl_irt_dev_filename __libnacl_irt_dev_filename;
extern struct nacl_irt_memory __libnacl_irt_memory;

// Create function pointers to the REAL implementation
#define EXPAND_SYMBOL_LIST_OPERATION(OP) \
  OP(basic, exit);                       \
  OP(fdio, close);                       \
  OP(fdio, dup);                         \
  OP(fdio, dup2);                        \
  OP(fdio, read);                        \
  OP(fdio, write);                       \
  OP(fdio, seek);                        \
  OP(fdio, fstat);                       \
  OP(fdio, getdents);                    \
  OP(dev_fdio, fchdir);                  \
  OP(dev_fdio, fchmod);                  \
  OP(dev_fdio, fsync);                   \
  OP(dev_fdio, fdatasync);               \
  OP(dev_fdio, ftruncate);               \
  OP(dev_fdio, isatty);                  \
  OP(dev_filename, open);                \
  OP(dev_filename, stat);                \
  OP(dev_filename, mkdir);               \
  OP(dev_filename, rmdir);               \
  OP(dev_filename, chdir);               \
  OP(dev_filename, getcwd);              \
  OP(dev_filename, unlink);              \
  OP(dev_filename, truncate);            \
  OP(dev_filename, lstat);               \
  OP(dev_filename, link);                \
  OP(dev_filename, rename);              \
  OP(dev_filename, symlink);             \
  OP(dev_filename, chmod);               \
  OP(dev_filename, access);              \
  OP(dev_filename, readlink);            \
  OP(dev_filename, utimes);              \
  OP(memory, mmap);                      \
  OP(memory, munmap);

EXPAND_SYMBOL_LIST_OPERATION(DECLARE_REAL_PTR);

int WRAP(close)(int fd) {
  ERRNO_RTN(ki_close(fd));
}

int WRAP(dup)(int fd, int* newfd) {
  *newfd = ki_dup(fd);
  ERRNO_RTN(*newfd);
}

int WRAP(dup2)(int fd, int newfd) {
  newfd = ki_dup2(fd, newfd);
  ERRNO_RTN(newfd);
}

void WRAP(exit)(int status) {
  ki_exit(status);
}

int WRAP(read)(int fd, void* buf, size_t count, size_t* nread) {
  ssize_t signed_nread = ki_read(fd, buf, count);
  *nread = static_cast<size_t>(signed_nread);
  ERRNO_RTN(signed_nread);
}

int WRAP(write)(int fd, const void* buf, size_t count, size_t* nwrote) {
  ssize_t signed_nwrote = ki_write(fd, buf, count);
  *nwrote = static_cast<size_t>(signed_nwrote);
  ERRNO_RTN(signed_nwrote);
}

int WRAP(seek)(int fd, off_t offset, int whence, off_t* new_offset) {
  *new_offset = ki_lseek(fd, offset, whence);
  ERRNO_RTN(*new_offset);
}

int WRAP(fstat)(int fd, struct stat* buf) {
  ERRNO_RTN(ki_fstat(fd, buf));
}

int WRAP(getdents)(int fd, dirent* buf, size_t count, size_t* nread) {
  int rtn = ki_getdents(fd, buf, count);
  RTN_ERRNO_IF(rtn < 0);
  *nread = rtn;
  return 0;
}

int WRAP(fchdir)(int fd) {
  ERRNO_RTN(ki_fchdir(fd));
}

int WRAP(fchmod)(int fd, mode_t mode) {
  ERRNO_RTN(ki_fchmod(fd, mode));
}

int WRAP(fsync)(int fd) {
  ERRNO_RTN(ki_fsync(fd));
}

int WRAP(fdatasync)(int fd) {
  ERRNO_RTN(ki_fdatasync(fd));
}

int WRAP(ftruncate)(int fd, off_t length) {
  ERRNO_RTN(ki_ftruncate(fd, length));
}

int WRAP(isatty)(int fd, int* result) {
  *result = ki_isatty(fd);
  RTN_ERRNO_IF(*result == 0);
  return 0;
}

int WRAP(mmap)(void** addr,
               size_t length,
               int prot,
               int flags,
               int fd,
               off_t offset) {
  if (flags & MAP_ANONYMOUS)
    return REAL(mmap)(addr, length, prot, flags, fd, offset);

  *addr = ki_mmap(*addr, length, prot, flags, fd, offset);
  RTN_ERRNO_IF(*addr == (void*)-1);
  return 0;
}

int WRAP(munmap)(void* addr, size_t length) {
  // Always let the real munmap run on the address range. It is not an error if
  // there are no mapped pages in that range.
  ki_munmap(addr, length);
  return REAL(munmap)(addr, length);
}

int WRAP(open)(const char* pathname, int oflag, mode_t mode, int* newfd) {
  *newfd = ki_open(pathname, oflag, mode);
  ERRNO_RTN(*newfd);
}

int WRAP(stat)(const char* pathname, struct stat* buf) {
  ERRNO_RTN(ki_stat(pathname, buf));
}

int WRAP(mkdir)(const char* pathname, mode_t mode) {
  ERRNO_RTN(ki_mkdir(pathname, mode));
}

int WRAP(rmdir)(const char* pathname) {
  ERRNO_RTN(ki_rmdir(pathname));
}

int WRAP(chdir)(const char* pathname) {
  ERRNO_RTN(ki_chdir(pathname));
}

int WRAP(getcwd)(char* pathname, size_t len) {
  char* rtn = ki_getcwd(pathname, len);
  RTN_ERRNO_IF(NULL == rtn);
  return 0;
}

int WRAP(unlink)(const char* pathname) {
  ERRNO_RTN(ki_unlink(pathname));
}

int WRAP(truncate)(const char* pathname, off_t length) {
  ERRNO_RTN(ki_truncate(pathname, length));
}

int WRAP(lstat)(const char* pathname, struct stat* buf) {
  ERRNO_RTN(ki_lstat(pathname, buf));
}

int WRAP(link)(const char* pathname, const char* newpath) {
  ERRNO_RTN(ki_link(pathname, newpath));
}

int WRAP(rename)(const char* pathname, const char* newpath) {
  ERRNO_RTN(ki_rename(pathname, newpath));
}

int WRAP(symlink)(const char* pathname, const char* newpath) {
  ERRNO_RTN(ki_symlink(pathname, newpath));
}

int WRAP(chmod)(const char* pathname, mode_t mode) {
  ERRNO_RTN(ki_chmod(pathname, mode));
}

int WRAP(access)(const char* pathname, int amode) {
  ERRNO_RTN(ki_access(pathname, amode));
}

int WRAP(readlink)(const char* pathname,
                   char* buf,
                   size_t count,
                   size_t* nread) {
  int rtn = ki_readlink(pathname, buf, count);
  RTN_ERRNO_IF(rtn < 0);
  *nread = rtn;
  return 0;
}

int WRAP(utimes)(const char* pathname, const struct timeval times[2]) {
  ERRNO_RTN(ki_utimes(pathname, times));
}

static void assign_real_pointers() {
  static bool assigned = false;
  if (!assigned) {
    __libnacl_irt_dev_filename_init();
    __libnacl_irt_dev_fdio_init();
    EXPAND_SYMBOL_LIST_OPERATION(ASSIGN_REAL_PTR)
    assigned = true;
  }
}

#define CHECK_REAL(func)    \
  if (!REAL(func)) {        \
    assign_real_pointers(); \
    if (!REAL(func))        \
      return ENOSYS;        \
  }

// "real" functions, i.e. the unwrapped original functions.

int _real_close(int fd) {
  CHECK_REAL(close);
  return REAL(close)(fd);
}

void _real_exit(int status) {
  if (!REAL(exit))
    assign_real_pointers();
  REAL(exit)(status);
}

int _real_fstat(int fd, struct stat* buf) {
  CHECK_REAL(fstat);
  return REAL(fstat)(fd, buf);
}

int _real_isatty(int fd, int* result) {
  CHECK_REAL(isatty);
  // The real isatty function can be NULL (for example if we are running
  // within chrome).
  if (REAL(isatty) == NULL) {
    *result = 0;
    return ENOTTY;
  }
  return REAL(isatty)(fd, result);
}

int _real_getdents(int fd, void* nacl_buf, size_t nacl_count, size_t* nread) {
  CHECK_REAL(getdents);
  return REAL(getdents)(fd, static_cast<dirent*>(nacl_buf), nacl_count, nread);
}

int _real_lseek(int fd, off_t offset, int whence, off_t* new_offset) {
  CHECK_REAL(seek);
  return REAL(seek)(fd, offset, whence, new_offset);
}

int _real_mkdir(const char* pathname, mode_t mode) {
  CHECK_REAL(mkdir);
  return REAL(mkdir)(pathname, mode);
}

int _real_mmap(void** addr,
               size_t length,
               int prot,
               int flags,
               int fd,
               off_t offset) {
  CHECK_REAL(mmap);
  return REAL(mmap)(addr, length, prot, flags, fd, offset);
}

int _real_munmap(void* addr, size_t length) {
  CHECK_REAL(munmap);
  return REAL(munmap)(addr, length);
}

int _real_open(const char* pathname, int oflag, mode_t mode, int* newfd) {
  CHECK_REAL(open);
  return REAL(open)(pathname, oflag, mode, newfd);
}

int _real_open_resource(const char* file, int* fd) {
  return ENOSYS;
}

int _real_read(int fd, void* buf, size_t count, size_t* nread) {
  CHECK_REAL(read);
  return REAL(read)(fd, buf, count, nread);
}

int _real_rmdir(const char* pathname) {
  CHECK_REAL(rmdir);
  return REAL(rmdir)(pathname);
}

int _real_write(int fd, const void* buf, size_t count, size_t* nwrote) {
  CHECK_REAL(write);
  return REAL(write)(fd, buf, count, nwrote);
}

int _real_getcwd(char* pathname, size_t len) {
  CHECK_REAL(getcwd);
  return REAL(getcwd)(pathname, len);
}

static bool s_wrapped = false;

void kernel_wrap_init() {
  if (!s_wrapped) {
    LOG_TRACE("kernel_wrap_init");
    assign_real_pointers();
    EXPAND_SYMBOL_LIST_OPERATION(USE_WRAP)
    s_wrapped = true;
  }
}

void kernel_wrap_uninit() {
  if (s_wrapped) {
    LOG_TRACE("kernel_wrap_uninit");
    EXPAND_SYMBOL_LIST_OPERATION(USE_REAL)
    s_wrapped = false;
  }
}

EXTERN_C_END

#endif  // defined(__native_client__) && !defined(__GLIBC__) ...
