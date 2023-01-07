/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* NACL_IO_IRT_EXT is defined in this header */
#include "nacl_io/kernel_wrap.h"

/*
 * The entire file is wrapped in this #if. We do this so this .c file can
 * always be compiled.
 */
#if defined(NACL_IO_IRT_EXT)

#include <assert.h>
#include <errno.h>
#include <irt.h>
#include <irt_dev.h>
#include <irt_extension.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/log.h"
#include "nacl_io/nacl_abi_types.h"

/*
 * The following macros are used to interfact with IRT interfaces.
 */
/* This macro defines an interfact structure, use as a regular type. */
#define NACL_IRT_INTERFACE(interface_type) \
  struct nacl_io_##interface_type {        \
    const char *query_string;              \
    bool queried;                          \
    bool initialized;                      \
    struct interface_type interface;       \
  }

/* This macro unconditionally initializes an interface (do not use directly). */
#define INIT_INTERFACE_BARE(interface_struct)                   \
  if (!interface_struct.queried) {                              \
    const size_t bytes __attribute__((unused)) =                \
      nacl_interface_query(interface_struct.query_string,       \
                           &interface_struct.interface,         \
                           sizeof(interface_struct.interface)); \
    interface_struct.queried = true;                            \
    interface_struct.initialized =                              \
      (bytes == sizeof(interface_struct.interface));            \
  }

/* This macro initializes an interface and does not handle errors. */
#define INIT_INTERFACE(interface_struct)   \
  do {                                     \
    INIT_INTERFACE_BARE(interface_struct); \
  } while (false)

/* This macro initializes an interface and returns ENOSYS on failure. */
#define INIT_INTERFACE_ENOSYS(interface_struct) \
  do {                                          \
    INIT_INTERFACE_BARE(interface_struct);      \
    if (!interface_struct.initialized)          \
      return ENOSYS;                            \
  } while (false)

/* This macro initializes an interface and asserts on failure. */
#define INIT_INTERFACE_ASSERT(interface_struct) \
  do {                                          \
    INIT_INTERFACE_BARE(interface_struct);      \
    assert(interface_struct.initialized);       \
  } while (false)

/* This macro supplies an IRT Extension interface and asserts on failure. */
#define EXT_SUPPLY_INTERFACE_ASSERT(interface_struct, supplied_struct) \
  do {                                                                 \
    const size_t bytes __attribute__((unused)) =                       \
      nacl_interface_ext_supply(interface_struct.query_string,         \
                                &supplied_struct,                      \
                                sizeof(supplied_struct));              \
    assert(bytes == sizeof(supplied_struct));                          \
  } while (false)


void stat_to_nacl_stat(const struct stat* buf, nacl_irt_stat_t* nacl_buf) {
  memset(nacl_buf, 0, sizeof(struct nacl_abi_stat));
  nacl_buf->nacl_abi_st_dev = buf->st_dev;
  nacl_buf->nacl_abi_st_ino = buf->st_ino;
  nacl_buf->nacl_abi_st_mode = buf->st_mode;
  nacl_buf->nacl_abi_st_nlink = buf->st_nlink;
  nacl_buf->nacl_abi_st_uid = buf->st_uid;
  nacl_buf->nacl_abi_st_gid = buf->st_gid;
  nacl_buf->nacl_abi_st_rdev = buf->st_rdev;
  nacl_buf->nacl_abi_st_size = buf->st_size;
  nacl_buf->nacl_abi_st_blksize = buf->st_blksize;
  nacl_buf->nacl_abi_st_blocks = buf->st_blocks;
  nacl_buf->nacl_abi_st_atime = buf->st_atim.tv_sec;
  nacl_buf->nacl_abi_st_atimensec = buf->st_atim.tv_nsec;
  nacl_buf->nacl_abi_st_mtime = buf->st_mtim.tv_sec;
  nacl_buf->nacl_abi_st_mtimensec = buf->st_mtim.tv_nsec;
  nacl_buf->nacl_abi_st_ctime = buf->st_ctim.tv_sec;
  nacl_buf->nacl_abi_st_ctimensec = buf->st_ctim.tv_nsec;
}

void nacl_stat_to_stat(const nacl_irt_stat_t* nacl_buf, struct stat* buf) {
  memset(buf, 0, sizeof(struct stat));
  buf->st_dev = nacl_buf->nacl_abi_st_dev;
  buf->st_ino = nacl_buf->nacl_abi_st_ino;
  buf->st_mode = nacl_buf->nacl_abi_st_mode;
  buf->st_nlink = nacl_buf->nacl_abi_st_nlink;
  buf->st_uid = nacl_buf->nacl_abi_st_uid;
  buf->st_gid = nacl_buf->nacl_abi_st_gid;
  buf->st_rdev = nacl_buf->nacl_abi_st_rdev;
  buf->st_size = nacl_buf->nacl_abi_st_size;
  buf->st_blksize = nacl_buf->nacl_abi_st_blksize;
  buf->st_blocks = nacl_buf->nacl_abi_st_blocks;
  buf->st_atim.tv_sec = nacl_buf->nacl_abi_st_atime;
  buf->st_atim.tv_nsec = nacl_buf->nacl_abi_st_atimensec;
  buf->st_mtim.tv_sec = nacl_buf->nacl_abi_st_mtime;
  buf->st_mtim.tv_nsec = nacl_buf->nacl_abi_st_mtimensec;
  buf->st_ctim.tv_sec = nacl_buf->nacl_abi_st_ctime;
  buf->st_ctim.tv_nsec = nacl_buf->nacl_abi_st_ctimensec;
}

/*
 * IRT interfaces as declared in irt.h.
 */
static NACL_IRT_INTERFACE(nacl_irt_basic) s_irt_basic = {
  NACL_IRT_BASIC_v0_1,
};

static NACL_IRT_INTERFACE(nacl_irt_fdio) s_irt_fdio = {
  NACL_IRT_FDIO_v0_1,
};

static NACL_IRT_INTERFACE(nacl_irt_memory) s_irt_memory = {
  NACL_IRT_MEMORY_v0_3,
};

static NACL_IRT_INTERFACE(nacl_irt_resource_open) s_irt_resource_open = {
  NACL_IRT_RESOURCE_OPEN_v0_1,
};

/*
 * IRT Dev interfaces as declared in irt_dev.h.
 */
static NACL_IRT_INTERFACE(nacl_irt_dev_fdio) s_irt_dev_fdio = {
  NACL_IRT_DEV_FDIO_v0_3,
};

static NACL_IRT_INTERFACE(nacl_irt_dev_filename) s_irt_dev_filename = {
  NACL_IRT_DEV_FILENAME_v0_3,
};

static bool s_wrapped = false;

/*
 * Functions for the nacl_irt_dev_fdio interface.
 */
static int ext_close(int fd) {
  ERRNO_RTN(ki_close(fd));
}

static int ext_dup(int fd, int *newfd) {
  *newfd = ki_dup(fd);
  ERRNO_RTN(*newfd);
}

static int ext_dup2(int fd, int newfd) {
  newfd = ki_dup2(fd, newfd);
  ERRNO_RTN(newfd);
}

static int ext_read(int fd, void *buf, size_t count, size_t *nread) {
  ssize_t signed_nread = ki_read(fd, buf, count);
  *nread = (size_t) signed_nread;
  ERRNO_RTN(signed_nread);
}

static int ext_write(int fd, const void *buf, size_t count, size_t *nwrote) {
  ssize_t signed_nwrote = ki_write(fd, buf, count);
  *nwrote = (size_t) signed_nwrote;
  ERRNO_RTN(signed_nwrote);
}

static int ext_seek(int fd, nacl_irt_off_t offset, int whence,
                    nacl_irt_off_t *new_offset) {
  *new_offset = ki_lseek(fd, offset, whence);
  ERRNO_RTN(*new_offset);
}

static int ext_fstat(int fd, nacl_irt_stat_t *nacl_buf) {
  struct stat buf;
  if (ki_fstat(fd, &buf)) {
    return errno;
  }
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

static int ext_getdents(int fd, struct dirent *ents, size_t count,
                        size_t *nread) {
  int rtn = ki_getdents(fd, ents, count);
  RTN_ERRNO_IF(rtn < 0);
  *nread = rtn;
  return 0;
}

/*
 * Functions for the nacl_irt_memory interface.
 */
static int ext_mmap(void **addr, size_t len, int prot, int flags, int fd,
                    nacl_irt_off_t off) {
  if (flags & MAP_ANONYMOUS)
    return s_irt_memory.interface.mmap(addr, len, prot, flags, fd, off);

  *addr = ki_mmap(*addr, len, prot, flags, fd, off);
  RTN_ERRNO_IF(*addr == (void*)-1);
  return 0;
}

static int ext_munmap(void *addr, size_t length) {
  /*
   * Always let the real munmap run on the address range. It is not an error if
   * there are no mapped pages in that range.
   */
  ki_munmap(addr, length);
  return s_irt_memory.interface.munmap(addr, length);
}

/*
 * Extra functions for the nacl_irt_dev_fdio interface.
 */
static int ext_fchdir(int fd) {
  ERRNO_RTN(ki_fchdir(fd));
}

static int ext_fchmod(int fd, mode_t mode) {
  ERRNO_RTN(ki_fchmod(fd, mode));
}

static int ext_fsync(int fd) {
  ERRNO_RTN(ki_fsync(fd));
}

static int ext_fdatasync(int fd) {
  ERRNO_RTN(ki_fdatasync(fd));
}

static int ext_ftruncate(int fd, nacl_irt_off_t length) {
  ERRNO_RTN(ki_ftruncate(fd, length));
}

static int ext_isatty(int fd, int *result) {
  *result = ki_isatty(fd);
  RTN_ERRNO_IF(*result == 0);
  return 0;
}

/*
 * Functions for the nacl_irt_dev_filename interface.
 */
static int ext_open(const char *pathname, int oflag, mode_t cmode, int *newfd) {
  *newfd = ki_open(pathname, oflag, cmode);
  ERRNO_RTN(*newfd);
}

static int ext_stat(const char *pathname, nacl_irt_stat_t *nacl_buf) {
  struct stat buf;
  if (ki_stat(pathname, &buf)) {
    return errno;
  }
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

static int ext_mkdir(const char *pathname, mode_t mode) {
  ERRNO_RTN(ki_mkdir(pathname, mode));
}

static int ext_rmdir(const char *pathname) {
  ERRNO_RTN(ki_rmdir(pathname));
}

static int ext_chdir(const char *pathname) {
  ERRNO_RTN(ki_chdir(pathname));
}

static int ext_getcwd(char *pathname, size_t len) {
  char *rtn = ki_getcwd(pathname, len);
  RTN_ERRNO_IF(NULL == rtn);
  return 0;
}

static int ext_unlink(const char *pathname) {
  ERRNO_RTN(ki_unlink(pathname));
}

static int ext_truncate(const char *pathname, nacl_irt_off_t length) {
  ERRNO_RTN(ki_truncate(pathname, length));
}

static int ext_lstat(const char *pathname, nacl_irt_stat_t *nacl_buf) {
  struct stat buf;
  if (ki_lstat(pathname, &buf)) {
    return errno;
  }
  stat_to_nacl_stat(&buf, nacl_buf);
  return 0;
}

static int ext_link(const char *pathname, const char *newpath) {
  ERRNO_RTN(ki_link(pathname, newpath));
}

static int ext_rename(const char *pathname, const char *newpath) {
  ERRNO_RTN(ki_rename(pathname, newpath));
}

static int ext_symlink(const char *pathname, const char *newpath) {
  ERRNO_RTN(ki_symlink(pathname, newpath));
}

static int ext_chmod(const char *pathname, mode_t mode) {
  ERRNO_RTN(ki_chmod(pathname, mode));
}

static int ext_access(const char *pathname, int amode) {
  ERRNO_RTN(ki_access(pathname, amode));
}

static int ext_readlink(const char *pathname, char *buf, size_t count,
                        size_t *nread) {
  int rtn = ki_readlink(pathname, buf, count);
  RTN_ERRNO_IF(rtn < 0);
  *nread = rtn;
  return 0;
}

static int ext_utimes(const char *pathname, const struct timeval *times) {
  ERRNO_RTN(ki_utimes(pathname, times));
}

/*
 * Functions declared inside of kernel_wrap_real.h.
 */

int _real_close(int fd) {
  INIT_INTERFACE_ENOSYS(s_irt_fdio);
  return s_irt_fdio.interface.close(fd);
}

void _real_exit(int status) {
  INIT_INTERFACE_ASSERT(s_irt_basic);
  return s_irt_basic.interface.exit(status);
}

int _real_fstat(int fd, struct stat *buf) {
  INIT_INTERFACE_ENOSYS(s_irt_fdio);
  nacl_irt_stat_t nacl_buf;
  int err = s_irt_fdio.interface.fstat(fd, &nacl_buf);
  if (err) {
    errno = err;
    return -1;
  }
  nacl_stat_to_stat(&nacl_buf, buf);
  return 0;
}

int _real_getdents(int fd, void *nacl_buf, size_t nacl_count, size_t *nread) {
  INIT_INTERFACE_ENOSYS(s_irt_fdio);
  return s_irt_fdio.interface.getdents(fd, (struct dirent *) nacl_buf,
                                       nacl_count, nread);
}

int _real_isatty(int fd, int *result) {
  INIT_INTERFACE_ENOSYS(s_irt_dev_fdio);
  return s_irt_dev_fdio.interface.isatty(fd, result);
}

int _real_lseek(int fd, int64_t offset, int whence, int64_t *new_offset) {
  INIT_INTERFACE_ENOSYS(s_irt_fdio);
  return s_irt_fdio.interface.seek(fd, offset, whence, new_offset);
}

int _real_mkdir(const char *pathname, mode_t mode) {
  INIT_INTERFACE_ENOSYS(s_irt_dev_filename);
  return s_irt_dev_filename.interface.mkdir(pathname, mode);
}

int _real_mmap(void **addr,
               size_t length,
               int prot,
               int flags,
               int fd,
               int64_t offset) {
  INIT_INTERFACE_ENOSYS(s_irt_memory);
  return s_irt_memory.interface.mmap(addr, length, prot, flags, fd, offset);
}

int _real_munmap(void *addr, size_t length) {
  INIT_INTERFACE_ENOSYS(s_irt_memory);
  return s_irt_memory.interface.munmap(addr, length);
}

int _real_open(const char *pathname, int oflag, mode_t mode, int *newfd) {
  INIT_INTERFACE_ENOSYS(s_irt_dev_filename);
  return s_irt_dev_filename.interface.open(pathname, oflag, mode, newfd);
}

int _real_open_resource(const char *file, int *fd) {
  INIT_INTERFACE_ENOSYS(s_irt_resource_open);
  return s_irt_resource_open.interface.open_resource(file, fd);
}

int _real_read(int fd, void *buf, size_t count, size_t *nread) {
  INIT_INTERFACE_ENOSYS(s_irt_fdio);
  return s_irt_fdio.interface.read(fd, buf, count, nread);
}

int _real_rmdir(const char *pathname) {
  INIT_INTERFACE_ENOSYS(s_irt_dev_filename);
  return s_irt_dev_filename.interface.rmdir(pathname);
}

int _real_write(int fd, const void *buf, size_t count, size_t *nwrote) {
  INIT_INTERFACE_ENOSYS(s_irt_fdio);
  return s_irt_fdio.interface.write(fd, buf, count, nwrote);
}

int _real_getcwd(char *pathname, size_t len) {
  INIT_INTERFACE_ENOSYS(s_irt_dev_filename);
  return s_irt_dev_filename.interface.getcwd(pathname, len);
}

/*
 * Kernel Wrap init/uninit functions declared in kernel_wrap.h.
 */
void kernel_wrap_init() {
  if (!s_wrapped) {
    LOG_TRACE("kernel_wrap_init");

    /*
     * Register interfaces as listed in irt.h.
     */

    /* Register nacl_irt_basic interface. */
    INIT_INTERFACE_ASSERT(s_irt_basic);
    struct nacl_irt_basic basic_calls = {
      ki_exit,
      s_irt_basic.interface.gettod,
      s_irt_basic.interface.clock,
      s_irt_basic.interface.nanosleep,
      s_irt_basic.interface.sched_yield,
      s_irt_basic.interface.sysconf,
    };
    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_basic, basic_calls);

    /* Register nacl_irt_fdio interface. */
    INIT_INTERFACE(s_irt_fdio);
    struct nacl_irt_fdio fdio = {
      ext_close,
      ext_dup,
      ext_dup2,
      ext_read,
      ext_write,
      ext_seek,
      ext_fstat,
      ext_getdents,
    };
    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_fdio, fdio);

    /* Register nacl_irt_memory interface. */
    INIT_INTERFACE_ASSERT(s_irt_memory);
    struct nacl_irt_memory mem = {
      ext_mmap,
      ext_munmap,
      s_irt_memory.interface.mprotect,
    };
    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_memory, mem);

    /*
     * Register interfaces as listed in irt_dev.h.
     */

    /* Register nacl_irt_dev_fdio interface. */
    INIT_INTERFACE(s_irt_dev_fdio);
    struct nacl_irt_dev_fdio dev_fdio = {
      ext_close,
      ext_dup,
      ext_dup2,
      ext_read,
      ext_write,
      ext_seek,
      ext_fstat,
      ext_getdents,
      ext_fchdir,
      ext_fchmod,
      ext_fsync,
      ext_fdatasync,
      ext_ftruncate,
      ext_isatty,
    };
    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_dev_fdio, dev_fdio);

    /* Register nacl_irt_dev_filename interface. */
    INIT_INTERFACE(s_irt_dev_filename);
    struct nacl_irt_dev_filename dev_filename = {
      ext_open,
      ext_stat,
      ext_mkdir,
      ext_rmdir,
      ext_chdir,
      ext_getcwd,
      ext_unlink,
      ext_truncate,
      ext_lstat,
      ext_link,
      ext_rename,
      ext_symlink,
      ext_chmod,
      ext_access,
      ext_readlink,
      ext_utimes,
    };
    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_dev_filename, dev_filename);

    s_wrapped = true;
  }
}

void kernel_wrap_uninit() {
  if (s_wrapped) {
    LOG_TRACE("kernel_wrap_uninit");

    /* Register original IRT interfaces in irt.h. */
    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_basic,
                                s_irt_basic.interface);

    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_fdio,
                                s_irt_fdio.interface);

    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_memory,
                                s_irt_memory.interface);

    /*
     * Register optional original IRT dev interfaces in irt_dev.h, these
     * may or may not exist since they are dev interfaces. If they do not
     * exist go ahead and supply an empty interface as that's what they
     * were originally before we supplied any extension interfaces.
     */
    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_dev_fdio,
                                s_irt_dev_fdio.interface);

    EXT_SUPPLY_INTERFACE_ASSERT(s_irt_dev_filename,
                                s_irt_dev_filename.interface);

    s_wrapped = false;
  }
}

#endif
