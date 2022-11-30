// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_MOCK_KERNEL_PROXY_H_
#define TESTS_NACL_IO_TEST_MOCK_KERNEL_PROXY_H_

#include <sys/types.h>
#include <sys/stat.h>
#include "gmock/gmock.h"

#include "nacl_io/kernel_proxy.h"
#include "nacl_io/ossignal.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostermios.h"

class MockKernelProxy : public nacl_io::KernelProxy {
 public:
  MockKernelProxy() {}

  MOCK_METHOD2(access, int(const char*, int));
  MOCK_METHOD1(chdir, int(const char*));
  MOCK_METHOD2(chmod, int(const char*, mode_t));
  MOCK_METHOD3(chown, int(const char*, uid_t, gid_t));
  MOCK_METHOD1(close, int(int));
  MOCK_METHOD1(dup, int(int));
  MOCK_METHOD2(dup2, int(int, int));
  MOCK_METHOD1(fchdir, int(int));
  MOCK_METHOD2(fchmod, int(int, mode_t));
  MOCK_METHOD3(fchown, int(int, uid_t, gid_t));
  MOCK_METHOD3(fcntl, int(int, int, va_list));
  MOCK_METHOD1(fdatasync, int(int));
  MOCK_METHOD2(ftruncate, int(int, off_t));
  MOCK_METHOD2(fstat, int(int, struct stat*));
  MOCK_METHOD1(fsync, int(int));
  MOCK_METHOD2(futimens, int(int, const struct timespec[2]));
  MOCK_METHOD2(getcwd, char*(char*, size_t));
  MOCK_METHOD3(getdents, int(int, struct dirent*, unsigned int));
  MOCK_METHOD1(getwd, char*(char*));
  MOCK_METHOD3(ioctl, int(int, int, va_list));
  MOCK_METHOD1(isatty, int(int));
  MOCK_METHOD2(kill, int(int, int));
  MOCK_METHOD3(lchown, int(const char*, uid_t, gid_t));
  MOCK_METHOD2(link, int(const char*, const char*));
  MOCK_METHOD3(lseek, off_t(int, off_t, int));
  MOCK_METHOD2(lstat, int(const char*, struct stat*));
  MOCK_METHOD2(mkdir, int(const char*, mode_t));
  MOCK_METHOD6(mmap, void*(void*, size_t, int, int, int, size_t));
  MOCK_METHOD5(mount, int(const char*, const char*, const char*, unsigned long,
                          const void*));

#ifndef _NEWLIB_VERSION
  /*
   * Disable mocking of munmap under newlib since it can lead to deadlocks
   * in dlmalloc: free -> dlmalloc -> acquire lock -> munmap -> mock munmap ->
   * std::string -> new -> dlmalloc -> acquire lock (already held).
   */
  MOCK_METHOD2(munmap, int(void*, size_t));
#endif
  MOCK_METHOD3(open, int(const char*, int, mode_t));
  MOCK_METHOD1(open_resource, int(const char*));
  MOCK_METHOD1(pipe, int(int[2]));
  MOCK_METHOD3(read, ssize_t(int, void*, size_t));
  MOCK_METHOD3(readlink, int(const char*, char*, size_t count));
  MOCK_METHOD1(remove, int(const char*));
  MOCK_METHOD2(rename, int(const char*, const char*));
  MOCK_METHOD1(rmdir, int(const char*));
  MOCK_METHOD3(sigaction, int(int, const struct sigaction*, struct sigaction*));
  MOCK_METHOD2(stat, int(const char*, struct stat*));
  MOCK_METHOD2(symlink, int(const char*, const char*));
  MOCK_METHOD2(tcflush, int(int, int));
  MOCK_METHOD2(tcgetattr, int(int, struct termios*));
  MOCK_METHOD3(tcsetattr, int(int, int, const struct termios*));
  MOCK_METHOD2(truncate, int(const char*, off_t));
  MOCK_METHOD1(umount, int(const char*));
  MOCK_METHOD1(unlink, int(const char*));
  MOCK_METHOD2(utimens, int(const char*, const struct timespec[2]));
  MOCK_METHOD3(write, ssize_t(int, const void*, size_t));

#ifdef PROVIDES_SOCKET_API
  MOCK_METHOD3(poll, int(struct pollfd*, nfds_t, int));
  MOCK_METHOD5(select, int(int, fd_set*, fd_set*, fd_set*, struct timeval*));

  // Socket support functions
  MOCK_METHOD3(accept, int(int, struct sockaddr*, socklen_t*));
  MOCK_METHOD3(bind, int(int, const struct sockaddr*, socklen_t));
  MOCK_METHOD3(connect, int(int, const struct sockaddr*, socklen_t));
  MOCK_METHOD1(gethostbyname, struct hostent*(const char*));
  MOCK_METHOD3(getpeername, int(int, struct sockaddr*, socklen_t*));
  MOCK_METHOD3(getsockname, int(int, struct sockaddr*, socklen_t*));
  MOCK_METHOD5(getsockopt, int(int, int, int, void*, socklen_t*));
  MOCK_METHOD2(listen, int(int, int));
  MOCK_METHOD4(recv, ssize_t(int, void*, size_t, int));
  MOCK_METHOD6(recvfrom, ssize_t(int, void*, size_t, int,
                                 struct sockaddr*, socklen_t*));
  MOCK_METHOD3(recvmsg, ssize_t(int, struct msghdr*, int));
  MOCK_METHOD4(send, ssize_t(int, const void*, size_t, int));
  MOCK_METHOD6(sendto, ssize_t(int, const void*, size_t, int,
                               const struct sockaddr*, socklen_t));
  MOCK_METHOD3(sendmsg, ssize_t(int, const struct msghdr*, int));
  MOCK_METHOD5(setsockopt, int(int, int, int, const void*, socklen_t));
  MOCK_METHOD2(shutdown, int(int, int));
  MOCK_METHOD3(socket, int(int, int, int));
  MOCK_METHOD4(socketpair, int(int, int, int, int*));
#endif // PROVIDES_SOCKET_API
};

#endif  // TESTS_NACL_IO_TEST_MOCK_KERNEL_PROXY_H_
