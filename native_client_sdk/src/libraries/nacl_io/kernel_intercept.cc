// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/kernel_intercept.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "nacl_io/kernel_proxy.h"
#include "nacl_io/kernel_wrap.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/log.h"
#include "nacl_io/osmman.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostime.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/real_pepper_interface.h"

using namespace nacl_io;

#define ON_NOSYS_RETURN(x)    \
  if (!ki_is_initialized()) { \
    errno = ENOSYS;           \
    return x;                 \
  }

#define TRACE_KP_CALLS 0

#if TRACE_KP_CALLS
#define KP_TRACE nacl_io_log
#else
#define KP_TRACE(...)
#endif

#define KP_CALL(METHOD, ARGS) \
  ON_NOSYS_RETURN(-1); \
  int rtn = s_state.kp-> METHOD ARGS; \
  KP_TRACE("ki_" #METHOD " -> %d\n", rtn); \
  return rtn;

struct KernelInterceptState {
  KernelProxy* kp;
  PepperInterface* ppapi;
  bool kp_owned;
};

static KernelInterceptState s_state;

// The the test code we want to be able to save the previous kernel
// proxy when intialising and restore it on uninit.
static KernelInterceptState s_saved_state;

int ki_push_state_for_testing() {
  assert(s_saved_state.kp == NULL);
  if (s_saved_state.kp != NULL)
    return 1;
  s_saved_state = s_state;
  s_state.kp = NULL;
  s_state.ppapi = NULL;
  s_state.kp_owned = false;
  return 0;
}

static void ki_pop_state() {
  // Swap out the KernelProxy. This will normally reset the
  // proxy to NULL, aside from in test code that has called
  // ki_push_state_for_testing().
  s_state = s_saved_state;
  s_saved_state.kp = NULL;
  s_saved_state.ppapi = NULL;
  s_saved_state.kp_owned = false;
}

int ki_pop_state_for_testing() {
  ki_pop_state();
  return 0;
}

int ki_init(void* kp) {
  LOG_TRACE("ki_init: %p", kp);
  return ki_init_ppapi(kp, 0, NULL);
}

int ki_init_ppapi(void* kp,
                  PP_Instance instance,
                  PPB_GetInterface get_browser_interface) {
  assert(!s_state.kp);
  if (s_state.kp != NULL)
    return 1;
  PepperInterface* ppapi = NULL;
  if (instance && get_browser_interface) {
    ppapi = new RealPepperInterface(instance, get_browser_interface);
    s_state.ppapi = ppapi;
  }
  int rtn = ki_init_interface(kp, ppapi);
  return rtn;
}

int ki_init_interface(void* kp, void* pepper_interface) {
  LOG_TRACE("ki_init_interface: %p %p", kp, pepper_interface);
  assert(!s_state.kp);
  if (s_state.kp != NULL)
    return 1;
  PepperInterface* ppapi = static_cast<PepperInterface*>(pepper_interface);
  kernel_wrap_init();

  if (kp == NULL) {
    s_state.kp = new KernelProxy();
    s_state.kp_owned = true;
  } else {
    s_state.kp = static_cast<KernelProxy*>(kp);
    s_state.kp_owned = false;
  }

  if (s_state.kp->Init(ppapi) != 0)
    return 1;

  return 0;
}

int ki_is_initialized() {
  return s_state.kp != NULL;
}

int ki_uninit() {
  LOG_TRACE("ki_uninit");
  assert(s_state.kp);
  if (s_state.kp == NULL)
    return 1;

  if (s_saved_state.kp == NULL)
    kernel_wrap_uninit();

  // If we are going to delete the KernelProxy don't do it
  // until we've swapped it out.
  KernelInterceptState state_to_delete = s_state;

  ki_pop_state();

  if (state_to_delete.kp_owned)
    delete state_to_delete.kp;

  delete state_to_delete.ppapi;
  return 0;
}

nacl_io::KernelProxy* ki_get_proxy() {
  return s_state.kp;
}

void ki_exit(int status) {
  KP_TRACE("ki_exit\n");
  if (ki_is_initialized())
    s_state.kp->exit(status);

  _real_exit(status);
}

char* ki_getcwd(char* buf, size_t size) {
  ON_NOSYS_RETURN(NULL);
  KP_TRACE("ki_getcwd\n");
  return s_state.kp->getcwd(buf, size);
}

char* ki_getwd(char* buf) {
  ON_NOSYS_RETURN(NULL);
  KP_TRACE("ki_getwd\n");
  return s_state.kp->getwd(buf);
}

int ki_chdir(const char* path) {
  KP_CALL(chdir, (path));
}

int ki_dup(int oldfd) {
  KP_CALL(dup, (oldfd));
}

int ki_dup2(int oldfd, int newfd) {
  KP_CALL(dup2, (oldfd, newfd));
}

int ki_chmod(const char* path, mode_t mode) {
  KP_CALL(chmod, (path, mode));
}

int ki_fchdir(int fd) {
  KP_CALL(fchdir, (fd));
}

int ki_fchmod(int fd, mode_t mode) {
  KP_CALL(fchmod, (fd, mode));
}

int ki_stat(const char* path, struct stat* buf) {
  KP_CALL(stat, (path, buf));
}

int ki_mkdir(const char* path, mode_t mode) {
  KP_CALL(mkdir, (path, mode));
}

int ki_rmdir(const char* path) {
  KP_CALL(rmdir, (path));
}

int ki_mount(const char* source,
             const char* target,
             const char* filesystemtype,
             unsigned long mountflags,
             const void* data) {
  KP_CALL(mount, (source, target, filesystemtype, mountflags, data));
}

int ki_umount(const char* path) {
  KP_CALL(umount, (path));
}

int ki_open(const char* path, int oflag, mode_t mode) {
  KP_CALL(open, (path, oflag, mode));
}

int ki_pipe(int pipefds[2]) {
  KP_CALL(pipe, (pipefds));
}

ssize_t ki_read(int fd, void* buf, size_t nbyte) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_read\n");
  return s_state.kp->read(fd, buf, nbyte);
}

ssize_t ki_write(int fd, const void* buf, size_t nbyte) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_write\n");
  return s_state.kp->write(fd, buf, nbyte);
}

int ki_fstat(int fd, struct stat* buf) {
  KP_CALL(fstat, (fd, buf));
}

int ki_getdents(int fd, struct dirent* buf, unsigned int count) {
  KP_CALL(getdents, (fd, buf, count));
}

int ki_ftruncate(int fd, off_t length) {
  KP_CALL(ftruncate, (fd, length));
}

int ki_fsync(int fd) {
  KP_CALL(fsync, (fd));
}

int ki_fdatasync(int fd) {
  KP_CALL(fdatasync, (fd));
}

int ki_isatty(int fd) {
  ON_NOSYS_RETURN(0);
  KP_TRACE("ki_isatty\n");
  return s_state.kp->isatty(fd);
}

int ki_close(int fd) {
  KP_CALL(close, (fd));
}

off_t ki_lseek(int fd, off_t offset, int whence) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_lseek\n");
  return s_state.kp->lseek(fd, offset, whence);
}

int ki_remove(const char* path) {
  KP_CALL(remove, (path));
}

int ki_unlink(const char* path) {
  KP_CALL(unlink, (path));
}

int ki_truncate(const char* path, off_t length) {
  KP_CALL(truncate, (path, length));
}

int ki_lstat(const char* path, struct stat* buf) {
  KP_CALL(lstat, (path, buf));
}

int ki_link(const char* oldpath, const char* newpath) {
  KP_CALL(link, (oldpath, newpath));
}

int ki_rename(const char* path, const char* newpath) {
  KP_CALL(rename, (path, newpath));
}

int ki_symlink(const char* oldpath, const char* newpath) {
  KP_CALL(symlink, (oldpath, newpath));
}

int ki_access(const char* path, int amode) {
  KP_CALL(access, (path, amode));
}

int ki_readlink(const char* path, char* buf, size_t count) {
  KP_CALL(readlink, (path, buf, count));
}

int ki_utimes(const char* path, const struct timeval times[2]) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_utimes");
  // Implement in terms of utimens.
  if (!times) {
    return s_state.kp->utimens(path, NULL);
  }

  struct timespec ts[2];
  ts[0].tv_sec = times[0].tv_sec;
  ts[0].tv_nsec = times[0].tv_usec * 1000;
  ts[1].tv_sec = times[1].tv_sec;
  ts[1].tv_nsec = times[1].tv_usec * 1000;
  return s_state.kp->utimens(path, ts);
}

int ki_futimes(int fd, const struct timeval times[2]) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_futimes");
  // Implement in terms of futimens.
  if (!times) {
    return s_state.kp->futimens(fd, NULL);
  }

  struct timespec ts[2];
  ts[0].tv_sec = times[0].tv_sec;
  ts[0].tv_nsec = times[0].tv_usec * 1000;
  ts[1].tv_sec = times[1].tv_sec;
  ts[1].tv_nsec = times[1].tv_usec * 1000;
  return s_state.kp->futimens(fd, ts);
}

void* ki_mmap(void* addr,
              size_t length,
              int prot,
              int flags,
              int fd,
              off_t offset) {
  ON_NOSYS_RETURN(MAP_FAILED);
  KP_TRACE("ki_mmap\n");
  return s_state.kp->mmap(addr, length, prot, flags, fd, offset);
}

int ki_munmap(void* addr, size_t length) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_munmap\n");
  return s_state.kp->munmap(addr, length);
}

int ki_open_resource(const char* file) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_open_resource\n");
  return s_state.kp->open_resource(file);
}

int ki_fcntl(int d, int request, va_list args) {
  KP_CALL(fcntl, (d, request, args));
}

int ki_ioctl(int d, int request, va_list args) {
  KP_CALL(ioctl, (d, request, args));
}

int ki_chown(const char* path, uid_t owner, gid_t group) {
  KP_CALL(chown, (path, owner, group));
}

int ki_fchown(int fd, uid_t owner, gid_t group) {
  KP_CALL(fchown, (fd, owner, group));
}

int ki_lchown(const char* path, uid_t owner, gid_t group) {
  KP_CALL(lchown, (path, owner, group));
}

int ki_utime(const char* filename, const struct utimbuf* times) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_utime\n");
  // Implement in terms of utimens.
  if (!times) {
    return s_state.kp->utimens(filename, NULL);
  }

  struct timespec ts[2];
  ts[0].tv_sec = times->actime;
  ts[0].tv_nsec = 0;
  ts[1].tv_sec = times->modtime;
  ts[1].tv_nsec = 0;
  return s_state.kp->utimens(filename, ts);
}

int ki_futimens(int fd, const struct timespec times[2]) {
  KP_CALL(futimens, (fd, times));
}

mode_t ki_umask(mode_t mask) {
  ON_NOSYS_RETURN(0);
  KP_TRACE("ki_umask\n");
  return s_state.kp->umask(mask);
}

int ki_poll(struct pollfd* fds, nfds_t nfds, int timeout) {
  KP_CALL(poll, (fds, nfds, timeout));
}

int ki_select(int nfds,
              fd_set* readfds,
              fd_set* writefds,
              fd_set* exceptfds,
              struct timeval* timeout) {
  KP_CALL(select, (nfds, readfds, writefds, exceptfds, timeout));
}

int ki_tcflush(int fd, int queue_selector) {
  KP_CALL(tcflush, (fd, queue_selector));
}

int ki_tcgetattr(int fd, struct termios* termios_p) {
  KP_CALL(tcgetattr, (fd, termios_p));
}

int ki_tcsetattr(int fd,
                 int optional_actions,
                 const struct termios* termios_p) {
  KP_CALL(tcsetattr, (fd, optional_actions, termios_p));
}

int ki_kill(pid_t pid, int sig) {
  KP_CALL(kill, (pid, sig));
}

int ki_killpg(pid_t pid, int sig) {
  errno = ENOSYS;
  return -1;
}

int ki_sigaction(int signum,
                 const struct sigaction* action,
                 struct sigaction* oaction) {
  KP_CALL(sigaction, (signum, action, oaction));
}

int ki_sigpause(int sigmask) {
  errno = ENOSYS;
  return -1;
}

int ki_sigpending(sigset_t* set) {
  errno = ENOSYS;
  return -1;
}

int ki_sigsuspend(const sigset_t* set) {
  errno = ENOSYS;
  return -1;
}

sighandler_t ki_signal(int signum, sighandler_t handler) {
  return ki_sigset(signum, handler);
}

sighandler_t ki_sigset(int signum, sighandler_t handler) {
  ON_NOSYS_RETURN(SIG_ERR);
  KP_TRACE("ki_sigset\n");
  // Implement sigset(2) in terms of sigaction(2).
  struct sigaction action;
  struct sigaction oaction;
  memset(&action, 0, sizeof(action));
  memset(&oaction, 0, sizeof(oaction));
  action.sa_handler = handler;
  int rtn = s_state.kp->sigaction(signum, &action, &oaction);
  if (rtn)
    return SIG_ERR;
  return oaction.sa_handler;
}

#ifdef PROVIDES_SOCKET_API
// Socket Functions
int ki_accept(int fd, struct sockaddr* addr, socklen_t* len) {
  KP_CALL(accept, (fd, addr, len));
}

int ki_bind(int fd, const struct sockaddr* addr, socklen_t len) {
  KP_CALL(bind, (fd, addr, len));
}

int ki_connect(int fd, const struct sockaddr* addr, socklen_t len) {
  KP_CALL(connect, (fd, addr, len));
}

struct hostent* ki_gethostbyname(const char* name) {
  ON_NOSYS_RETURN(NULL);
  return s_state.kp->gethostbyname(name);
}

int ki_getnameinfo(const struct sockaddr *sa,
                   socklen_t salen,
                   char *host,
                   size_t hostlen,
                   char *serv,
                   size_t servlen,
                   unsigned int flags) {
  ON_NOSYS_RETURN(EAI_SYSTEM);
  KP_TRACE("ki_getnameinfo\n");
  return s_state.kp->getnameinfo(sa, salen, host, hostlen, serv, servlen,
                                 flags);
}

int ki_getaddrinfo(const char* node,
                   const char* service,
                   const struct addrinfo* hints,
                   struct addrinfo** res) {
  ON_NOSYS_RETURN(EAI_SYSTEM);
  KP_TRACE("ki_getaddrinfo\n");
  return s_state.kp->getaddrinfo(node, service, hints, res);
}

void ki_freeaddrinfo(struct addrinfo* res) {
  KP_TRACE("ki_freeaddrinfo\n");
  s_state.kp->freeaddrinfo(res);
}

int ki_getpeername(int fd, struct sockaddr* addr, socklen_t* len) {
  KP_CALL(getpeername, (fd, addr, len));
}

int ki_getsockname(int fd, struct sockaddr* addr, socklen_t* len) {
  KP_CALL(getsockname, (fd, addr, len));
}

int ki_getsockopt(int fd, int lvl, int optname, void* optval, socklen_t* len) {
  KP_CALL(getsockopt, (fd, lvl, optname, optval, len));
}

int ki_listen(int fd, int backlog) {
  KP_CALL(listen, (fd, backlog));
}

ssize_t ki_recv(int fd, void* buf, size_t len, int flags) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_recv\n");
  return s_state.kp->recv(fd, buf, len, flags);
}

ssize_t ki_recvfrom(int fd,
                    void* buf,
                    size_t len,
                    int flags,
                    struct sockaddr* addr,
                    socklen_t* addrlen) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_recvfrom\n");
  return s_state.kp->recvfrom(fd, buf, len, flags, addr, addrlen);
}

ssize_t ki_recvmsg(int fd, struct msghdr* msg, int flags) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_recvmsg\n");
  return s_state.kp->recvmsg(fd, msg, flags);
}

ssize_t ki_send(int fd, const void* buf, size_t len, int flags) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_send\n");
  return s_state.kp->send(fd, buf, len, flags);
}

ssize_t ki_sendto(int fd,
                  const void* buf,
                  size_t len,
                  int flags,
                  const struct sockaddr* addr,
                  socklen_t addrlen) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_sendto\n");
  return s_state.kp->sendto(fd, buf, len, flags, addr, addrlen);
}

ssize_t ki_sendmsg(int fd, const struct msghdr* msg, int flags) {
  ON_NOSYS_RETURN(-1);
  KP_TRACE("ki_sendmsg\n");
  return s_state.kp->sendmsg(fd, msg, flags);
}

int ki_setsockopt(int fd,
                  int lvl,
                  int optname,
                  const void* optval,
                  socklen_t len) {
  KP_CALL(setsockopt, (fd, lvl, optname, optval, len));
}

int ki_shutdown(int fd, int how) {
  KP_CALL(shutdown, (fd, how));
}

int ki_socket(int domain, int type, int protocol) {
  KP_CALL(socket, (domain, type, protocol));
}

int ki_socketpair(int domain, int type, int protocol, int* sv) {
  KP_CALL(socketpair, (domain, type, protocol, sv));
}
#endif  // PROVIDES_SOCKET_API
