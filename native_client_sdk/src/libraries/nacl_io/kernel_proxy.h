// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_KERNEL_PROXY_H_
#define LIBRARIES_NACL_IO_KERNEL_PROXY_H_

#include <map>
#include <string>

#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/event_emitter.h"
#include "nacl_io/fs_factory.h"
#include "nacl_io/host_resolver.h"
#include "nacl_io/kernel_object.h"
#include "nacl_io/nacl_io.h"
#include "nacl_io/ossignal.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostypes.h"
#include "nacl_io/osutime.h"
#include "nacl_io/stream/stream_fs.h"

struct fuse_operations;
struct timeval;

namespace nacl_io {

class PepperInterface;


// KernelProxy provide one-to-one mapping for libc kernel calls.  Calls to the
// proxy will result in IO access to the provided Filesystem and Node objects.
//
// NOTE: The KernelProxy does not directly take any kernel locks, all locking
// is done by the parent class KernelObject. Instead, KernelProxy is
// responsible for taking the locks of the KernelHandle, and Node objects. For
// this reason, a KernelObject call should not be done while holding a handle
// or node lock. In addition, to ensure locking order, a KernelHandle lock
// must never be taken after taking the associated Node's lock.
//
// NOTE: The KernelProxy is the only class that should be setting errno. All
// other classes should return Error (as defined by nacl_io/error.h).
class KernelProxy : protected KernelObject {
 public:
  typedef std::map<std::string, FsFactory*> FsFactoryMap_t;

  KernelProxy();

  KernelProxy(const KernelProxy&) = delete;
  KernelProxy& operator=(const KernelProxy&) = delete;

  virtual ~KernelProxy();

  // |ppapi| may be NULL. If so, no filesystem that uses pepper calls can be
  // mounted.
  virtual Error Init(PepperInterface* ppapi);

  // Register/Unregister a new filesystem type. See the documentation in
  // nacl_io.h for more info.
  bool RegisterFsType(const char* fs_type, fuse_operations* fuse_ops);
  bool UnregisterFsType(const char* fs_type);

  void SetExitCallback(nacl_io_exit_callback_t exit_callback, void* user_data);

  void SetMountCallback(nacl_io_mount_callback_t mount_callback,
                        void* user_data);

  virtual int pipe(int pipefds[2]);

  // NaCl-only function to read resources specified in the NMF file.
  virtual int open_resource(const char* file);

  // KernelHandle and FD allocation and manipulation functions.
  virtual int open(const char* path, int open_flags, mode_t mode);
  virtual int close(int fd);
  virtual int dup(int fd);
  virtual int dup2(int fd, int newfd);

  virtual void exit(int status);

  // Path related System calls handled by KernelProxy (not filesystem-specific)
  virtual int chdir(const char* path);
  virtual char* getcwd(char* buf, size_t size);
  virtual char* getwd(char* buf);
  virtual int mount(const char* source,
                    const char* target,
                    const char* filesystemtype,
                    unsigned long mountflags,
                    const void* data);
  virtual int umount(const char* path);

  // Stub system calls that don't do anything (yet), handled by KernelProxy.
  virtual int chown(const char* path, uid_t owner, gid_t group);
  virtual int fchown(int fd, uid_t owner, gid_t group);
  virtual int lchown(const char* path, uid_t owner, gid_t group);

  // System calls that take a path as an argument: The kernel proxy will look
  // for the Node associated to the path. To find the node, the kernel proxy
  // calls the corresponding filesystem's GetNode() method. The corresponding
  // method will be called. If the node cannot be found, errno is set and -1 is
  // returned.
  virtual int chmod(const char* path, mode_t mode);
  virtual int mkdir(const char* path, mode_t mode);
  virtual int rmdir(const char* path);
  virtual int stat(const char* path, struct stat* buf);

  // System calls that take a file descriptor as an argument:
  // The kernel proxy will determine to which filesystem the file
  // descriptor's corresponding file handle belongs.  The
  // associated filesystem's function will be called.
  virtual ssize_t read(int fd, void* buf, size_t nbyte);
  virtual ssize_t write(int fd, const void* buf, size_t nbyte);

  virtual int fchmod(int fd, mode_t mode);
  virtual int fcntl(int fd, int request, va_list args);
  virtual int fstat(int fd, struct stat* buf);
  virtual int getdents(int fd, struct dirent* buf, unsigned int count);
  virtual int fchdir(int fd);
  virtual int ftruncate(int fd, off_t length);
  virtual int fsync(int fd);
  virtual int fdatasync(int fd);
  virtual int isatty(int fd);
  virtual int ioctl(int fd, int request, va_list args);
  virtual int futimens(int fd, const struct timespec times[2]);

  // lseek() relies on the filesystem's Stat() to determine whether or not the
  // file handle corresponding to fd is a directory
  virtual off_t lseek(int fd, off_t offset, int whence);

  // remove() uses the filesystem's GetNode() and Stat() to determine whether
  // or not the path corresponds to a directory or a file. The filesystem's
  // Rmdir() or Unlink() is called accordingly.
  virtual int remove(const char* path);
  // unlink() is a simple wrapper around the filesystem's Unlink function.
  virtual int unlink(const char* path);
  virtual int truncate(const char* path, off_t len);
  virtual int lstat(const char* path, struct stat* buf);
  virtual int rename(const char* path, const char* newpath);
  // access() uses the Filesystem's Stat().
  virtual int access(const char* path, int amode);
  virtual int readlink(const char* path, char* buf, size_t count);
  virtual int utimens(const char* path, const struct timespec times[2]);

  virtual int link(const char* oldpath, const char* newpath);
  virtual int symlink(const char* oldpath, const char* newpath);

  virtual void* mmap(void* addr,
                     size_t length,
                     int prot,
                     int flags,
                     int fd,
                     size_t offset);
  virtual int munmap(void* addr, size_t length);
  virtual int tcflush(int fd, int queue_selector);
  virtual int tcgetattr(int fd, struct termios* termios_p);
  virtual int tcsetattr(int fd,
                        int optional_actions,
                        const struct termios* termios_p);

  virtual int kill(pid_t pid, int sig);
  virtual int sigaction(int signum,
                        const struct sigaction* action,
                        struct sigaction* oaction);
  virtual mode_t umask(mode_t);

#ifdef PROVIDES_SOCKET_API
  virtual int select(int nfds,
                     fd_set* readfds,
                     fd_set* writefds,
                     fd_set* exceptfds,
                     struct timeval* timeout);

  virtual int poll(struct pollfd* fds, nfds_t nfds, int timeout);

  // Socket support functions
  virtual int accept(int fd, struct sockaddr* addr, socklen_t* len);
  virtual int bind(int fd, const struct sockaddr* addr, socklen_t len);
  virtual int connect(int fd, const struct sockaddr* addr, socklen_t len);
  virtual struct hostent* gethostbyname(const char* name);
  virtual void freeaddrinfo(struct addrinfo* res);
  virtual int getaddrinfo(const char* node,
                          const char* service,
                          const struct addrinfo* hints,
                          struct addrinfo** res);
  virtual int getnameinfo(const struct sockaddr *sa,
                          socklen_t salen,
                          char *host,
                          size_t hostlen,
                          char *serv,
                          size_t servlen,
                          int flags);
  virtual int getpeername(int fd, struct sockaddr* addr, socklen_t* len);
  virtual int getsockname(int fd, struct sockaddr* addr, socklen_t* len);
  virtual int getsockopt(int fd,
                         int lvl,
                         int optname,
                         void* optval,
                         socklen_t* len);
  virtual int listen(int fd, int backlog);
  virtual ssize_t recv(int fd, void* buf, size_t len, int flags);
  virtual ssize_t recvfrom(int fd,
                           void* buf,
                           size_t len,
                           int flags,
                           struct sockaddr* addr,
                           socklen_t* addrlen);
  // recvmsg ignores ancillary data.
  virtual ssize_t recvmsg(int fd, struct msghdr* msg, int flags);
  virtual ssize_t send(int fd, const void* buf, size_t len, int flags);
  virtual ssize_t sendto(int fd,
                         const void* buf,
                         size_t len,
                         int flags,
                         const struct sockaddr* addr,
                         socklen_t addrlen);
  // sendmsg ignores ancillary data.
  virtual ssize_t sendmsg(int fd, const struct msghdr* msg, int flags);
  virtual int setsockopt(int fd,
                         int lvl,
                         int optname,
                         const void* optval,
                         socklen_t len);
  virtual int shutdown(int fd, int how);
  virtual int socket(int domain, int type, int protocol);
  virtual int socketpair(int domain, int type, int protocol, int* sv);
#endif  // PROVIDES_SOCKET_API

 protected:
  Error MountInternal(const char* source,
                      const char* target,
                      const char* filesystemtype,
                      unsigned long mountflags,
                      const void* data,
                      bool create_fs_node,
                      ScopedFilesystem* out_filesystem);

  Error FutimensInternal(const ScopedNode& node,
                         const struct timespec times[2]);

  Error CreateFsNode(const ScopedFilesystem& fs);

 protected:
  FsFactoryMap_t factories_;
  sdk_util::ScopedRef<StreamFs> stream_fs_;
  sdk_util::ScopedRef<DevFs> dev_fs_;
  int dev_;
  PepperInterface* ppapi_;
  static KernelProxy* s_instance_;
  struct sigaction sigwinch_handler_;
  nacl_io_exit_callback_t exit_callback_;
  void* exit_callback_user_data_;
  nacl_io_mount_callback_t mount_callback_;
  void* mount_callback_user_data_;
#ifdef PROVIDES_SOCKET_API
  HostResolver host_resolver_;
#endif

#ifdef PROVIDES_SOCKET_API
  virtual int AcquireSocketHandle(int fd, ScopedKernelHandle* handle);
#endif

  ScopedEventEmitter signal_emitter_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_KERNEL_PROXY_H_
