/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_KERNEL_INTERCEPT_H_
#define LIBRARIES_NACL_IO_KERNEL_INTERCEPT_H_

#include <dirent.h>
#include <stdarg.h>
#include <sys/time.h>

#include <ppapi/c/ppb.h>
#include <ppapi/c/pp_instance.h>

#include "nacl_io/ossignal.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/osstat.h"
#include "nacl_io/ostermios.h"
#include "nacl_io/ostypes.h"
#include "nacl_io/osutime.h"
#include "sdk_util/macros.h"

EXTERN_C_BEGIN

#ifdef __cplusplus
namespace nacl_io {
class KernelProxy;
}
#endif

/*
 * The kernel intercept module provides a C->C++ thunk between the libc
 * kernel calls and the KernelProxy singleton.
 */

/*
 * ki_init must be called with an uninitialized KernelProxy object.  Calling
 * with NULL will instantiate a default kernel proxy object.  ki_init must
 * be called before any other ki_XXX function can be used.
 */
int ki_init(void* kernel_proxy);

/*
 * Saves the current internal state.  This is used by test code which can
 * use this to save the current state before calling ki_init().  The
 * pushed state is restored by ki_pop_state_for_testing() (or ki_uninit()).
 */
int ki_push_state_for_testing(void);

int ki_pop_state_for_testing(void);

int ki_init_ppapi(void* kernel_proxy,
                  PP_Instance instance,
                  PPB_GetInterface get_browser_interface);

/*
 * ki_init_interface() is a variant of ki_init() that can be called with
 * a PepperInterface object.
 */
int ki_init_interface(void* kernel_proxy, void* pepper_interface);

#ifdef __cplusplus
nacl_io::KernelProxy* ki_get_proxy();
#endif

int ki_is_initialized(void);
int ki_uninit(void);

int ki_chdir(const char* path);
void ki_exit(int status);
char* ki_getcwd(char* buf, size_t size);
char* ki_getwd(char* buf);
int ki_dup(int oldfd);
int ki_dup2(int oldfd, int newfd);
int ki_chmod(const char* path, mode_t mode);
int ki_fchdir(int fd);
int ki_fchmod(int fd, mode_t mode);
int ki_stat(const char* path, struct stat* buf);
int ki_mkdir(const char* path, mode_t mode);
int ki_rmdir(const char* path);
int ki_mount(const char* source,
             const char* target,
             const char* filesystemtype,
             unsigned long mountflags,
             const void* data);
int ki_umount(const char* path);
int ki_open(const char* path, int oflag, mode_t mode);
int ki_pipe(int pipefds[2]);
ssize_t ki_read(int fd, void* buf, size_t nbyte);
ssize_t ki_write(int fd, const void* buf, size_t nbyte);
int ki_fstat(int fd, struct stat* buf);
int ki_getdents(int fd, struct dirent* buf, unsigned int count);
int ki_fsync(int fd);
int ki_fdatasync(int fd);
int ki_ftruncate(int fd, off_t length);
int ki_isatty(int fd);
int ki_close(int fd);
off_t ki_lseek(int fd, off_t offset, int whence);
int ki_remove(const char* path);
int ki_unlink(const char* path);
int ki_truncate(const char* path, off_t length);
int ki_lstat(const char* path, struct stat* buf);
int ki_link(const char* oldpath, const char* newpath);
int ki_rename(const char* oldpath, const char* newpath);
int ki_symlink(const char* oldpath, const char* newpath);
int ki_access(const char* path, int amode);
int ki_readlink(const char* path, char* buf, size_t count);
int ki_utimes(const char* path, const struct timeval times[2]);
int ki_futimes(int fd, const struct timeval times[2]);
void* ki_mmap(void* addr,
              size_t length,
              int prot,
              int flags,
              int fd,
              off_t offset);
int ki_munmap(void* addr, size_t length);
int ki_open_resource(const char* file);
int ki_fcntl(int d, int request, va_list args);
int ki_ioctl(int d, int request, va_list args);
int ki_chown(const char* path, uid_t owner, gid_t group);
int ki_fchown(int fd, uid_t owner, gid_t group);
int ki_lchown(const char* path, uid_t owner, gid_t group);
int ki_utime(const char* filename, const struct utimbuf* times);
int ki_futimens(int fd, const struct timespec times[2]);
mode_t ki_umask(mode_t mask);

int ki_poll(struct pollfd* fds, nfds_t nfds, int timeout);
int ki_select(int nfds,
              fd_set* readfds,
              fd_set* writefds,
              fd_set* exceptfds,
              struct timeval* timeout);

int ki_tcflush(int fd, int queue_selector);
int ki_tcgetattr(int fd, struct termios* termios_p);
int ki_tcsetattr(int fd, int optional_actions, const struct termios* termios_p);
int ki_kill(pid_t pid, int sig);
int ki_killpg(pid_t pid, int sig);
int ki_sigaction(int signum,
                 const struct sigaction* action,
                 struct sigaction* oaction);
int ki_sigpause(int sigmask);
int ki_sigpending(sigset_t* set);
int ki_sigsuspend(const sigset_t* set);
sighandler_t ki_signal(int signum, sighandler_t handler);
sighandler_t ki_sigset(int signum, sighandler_t handler);

#ifdef PROVIDES_SOCKET_API
/* Socket Functions */
int ki_accept(int fd, struct sockaddr* addr, socklen_t* len);
int ki_bind(int fd, const struct sockaddr* addr, socklen_t len);
int ki_connect(int fd, const struct sockaddr* addr, socklen_t len);
void ki_freeaddrinfo(struct addrinfo* res);
int ki_getaddrinfo(const char* node,
                   const char* service,
                   const struct addrinfo* hints,
                   struct addrinfo** res);
struct hostent* ki_gethostbyname(const char* name);
int ki_getnameinfo(const struct sockaddr *sa,
                   socklen_t salen,
                   char *host,
                   size_t hostlen,
                   char *serv,
                   size_t servlen,
                   unsigned int flags);
int ki_getpeername(int fd, struct sockaddr* addr, socklen_t* len);
int ki_getsockname(int fd, struct sockaddr* addr, socklen_t* len);
int ki_getsockopt(int fd, int lvl, int optname, void* optval, socklen_t* len);
int ki_listen(int fd, int backlog);
ssize_t ki_recv(int fd, void* buf, size_t len, int flags);
ssize_t ki_recvfrom(int fd,
                    void* buf,
                    size_t len,
                    int flags,
                    struct sockaddr* addr,
                    socklen_t* addrlen);
ssize_t ki_recvmsg(int fd, struct msghdr* msg, int flags);
ssize_t ki_send(int fd, const void* buf, size_t len, int flags);
ssize_t ki_sendto(int fd,
                  const void* buf,
                  size_t len,
                  int flags,
                  const struct sockaddr* addr,
                  socklen_t addrlen);
ssize_t ki_sendmsg(int fd, const struct msghdr* msg, int flags);
int ki_setsockopt(int fd,
                  int lvl,
                  int optname,
                  const void* optval,
                  socklen_t len);
int ki_shutdown(int fd, int how);
int ki_socket(int domain, int type, int protocol);
int ki_socketpair(int domain, int type, int protocl, int* sv);
#endif  /* PROVIDES_SOCKET_API */

EXTERN_C_END

#endif  // LIBRARIES_NACL_IO_KERNEL_INTERCEPT_H_
