/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_NACL_ABI_TYPES_H_
#define LIBRARIES_NACL_IO_NACL_ABI_TYPES_H_

/*
 * copied from native_client/src/trusted/service_runtime/include/...
 * TODO(sbc): Figure out a way to avoid this duplication.
 */

#ifndef nacl_abi___dev_t_defined
#define nacl_abi___dev_t_defined
typedef __int64_t nacl_abi___dev_t;
#if !defined(NACL_IN_TOOLCHAIN_HEADERS)
typedef nacl_abi___dev_t nacl_abi_dev_t;
#endif
#endif

#ifndef nacl_abi___ino_t_defined
#define nacl_abi___ino_t_defined
typedef __uint64_t nacl_abi___ino_t;
#if !defined(NACL_IN_TOOLCHAIN_HEADERS)
typedef nacl_abi___ino_t nacl_abi_ino_t;
#endif
#endif

#ifndef nacl_abi___mode_t_defined
#define nacl_abi___mode_t_defined
typedef __uint32_t nacl_abi___mode_t;
#if !defined(NACL_IN_TOOLCHAIN_HEADERS)
typedef nacl_abi___mode_t nacl_abi_mode_t;
#endif
#endif

#ifndef nacl_abi___nlink_t_defined
#define nacl_abi___nlink_t_defined
typedef __uint32_t nacl_abi___nlink_t;
#if !defined(NACL_IN_TOOLCHAIN_HEADERS)
typedef nacl_abi___nlink_t nacl_abi_nlink_t;
#endif
#endif

#ifndef nacl_abi___uid_t_defined
#define nacl_abi___uid_t_defined
typedef __uint32_t nacl_abi___uid_t;
#if !defined(NACL_IN_TOOLCHAIN_HEADERS)
typedef nacl_abi___uid_t nacl_abi_uid_t;
#endif
#endif

#ifndef nacl_abi___gid_t_defined
#define nacl_abi___gid_t_defined
typedef __uint32_t nacl_abi___gid_t;
#if !defined(NACL_IN_TOOLCHAIN_HEADERS)
typedef nacl_abi___gid_t nacl_abi_gid_t;
#endif
#endif

#ifndef nacl_abi___off_t_defined
#define nacl_abi___off_t_defined
typedef __int64_t nacl_abi__off_t;
#if !defined(NACL_IN_TOOLCHAIN_HEADERS)
typedef nacl_abi__off_t nacl_abi_off_t;
#endif
#endif

#ifndef nacl_abi___off64_t_defined
#define nacl_abi___off64_t_defined
typedef __int64_t nacl_abi__off64_t;
#if !defined(NACL_IN_TOOLCHAIN_HEADERS)
typedef nacl_abi__off64_t nacl_abi_off64_t;
#endif
#endif

#ifndef nacl_abi___blksize_t_defined
#define nacl_abi___blksize_t_defined
typedef __int32_t nacl_abi___blksize_t;
typedef nacl_abi___blksize_t nacl_abi_blksize_t;
#endif

#ifndef nacl_abi___blkcnt_t_defined
#define nacl_abi___blkcnt_t_defined
typedef __int32_t nacl_abi___blkcnt_t;
typedef nacl_abi___blkcnt_t nacl_abi_blkcnt_t;
#endif

#ifndef nacl_abi___time_t_defined
#define nacl_abi___time_t_defined
typedef __int64_t       nacl_abi___time_t;
typedef nacl_abi___time_t nacl_abi_time_t;
#endif

#if !(__GLIBC__ == 2 && __GLIBC_MINOR__ == 9) && !defined(__BIONIC__)
struct nacl_abi_stat {  /* must be renamed when ABI is exported */
  nacl_abi_dev_t     nacl_abi_st_dev;       /* not implemented */
  nacl_abi_ino_t     nacl_abi_st_ino;       /* not implemented */
  nacl_abi_mode_t    nacl_abi_st_mode;      /* partially implemented. */
  nacl_abi_nlink_t   nacl_abi_st_nlink;     /* link count */
  nacl_abi_uid_t     nacl_abi_st_uid;       /* not implemented */
  nacl_abi_gid_t     nacl_abi_st_gid;       /* not implemented */
  nacl_abi_dev_t     nacl_abi_st_rdev;      /* not implemented */
  nacl_abi_off_t     nacl_abi_st_size;      /* object size */
  nacl_abi_blksize_t nacl_abi_st_blksize;   /* not implemented */
  nacl_abi_blkcnt_t  nacl_abi_st_blocks;    /* not implemented */
  nacl_abi_time_t    nacl_abi_st_atime;     /* access time */
  int64_t            nacl_abi_st_atimensec; /* possibly just pad */
  nacl_abi_time_t    nacl_abi_st_mtime;     /* modification time */
  int64_t            nacl_abi_st_mtimensec; /* possibly just pad */
  nacl_abi_time_t    nacl_abi_st_ctime;     /* inode change time */
  int64_t            nacl_abi_st_ctimensec; /* possibly just pad */
};
#endif

/* We need a way to define the maximum size of a name. */
#ifndef MAXNAMLEN
# ifdef NAME_MAX
#  define MAXNAMLEN NAME_MAX
# else
#  define MAXNAMLEN 255
# endif
#endif

struct nacl_abi_dirent {
  nacl_abi_ino_t nacl_abi_d_ino;
  nacl_abi_off_t nacl_abi_d_off;
  uint16_t       nacl_abi_d_reclen;
  char           nacl_abi_d_name[MAXNAMLEN + 1];
};

#endif  // LIBRARIES_NACL_IO_NACL_ABI_TYPES_H_
