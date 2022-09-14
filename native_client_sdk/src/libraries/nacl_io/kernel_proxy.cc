// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/kernel_proxy.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <iterator>
#include <string>

#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/filesystem.h"
#include "nacl_io/fusefs/fuse_fs_factory.h"
#include "nacl_io/googledrivefs/googledrivefs.h"
#include "nacl_io/host_resolver.h"
#include "nacl_io/html5fs/html5_fs.h"
#include "nacl_io/httpfs/http_fs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/log.h"
#include "nacl_io/memfs/mem_fs.h"
#include "nacl_io/node.h"
#include "nacl_io/osinttypes.h"
#include "nacl_io/osmman.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/osstat.h"
#include "nacl_io/passthroughfs/passthrough_fs.h"
#include "nacl_io/path.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/pipe/pipe_node.h"
#include "nacl_io/socket/tcp_node.h"
#include "nacl_io/socket/udp_node.h"
#include "nacl_io/socket/unix_node.h"
#include "nacl_io/stream/stream_fs.h"
#include "nacl_io/typed_fs_factory.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/ref_object.h"
#include "sdk_util/string_util.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

namespace nacl_io {

KernelProxy::KernelProxy()
    : dev_(0),
      ppapi_(NULL),
      exit_callback_(NULL),
      exit_callback_user_data_(NULL),
      mount_callback_(NULL),
      mount_callback_user_data_(NULL),
      signal_emitter_(new EventEmitter) {
  memset(&sigwinch_handler_, 0, sizeof(sigwinch_handler_));
  sigwinch_handler_.sa_handler = SIG_DFL;
}

KernelProxy::~KernelProxy() {
  // Clean up the FsFactories.
  for (FsFactoryMap_t::iterator i = factories_.begin(); i != factories_.end();
       ++i) {
    delete i->second;
  }
}

Error KernelProxy::Init(PepperInterface* ppapi) {
  Error rtn = 0;
  ppapi_ = ppapi;
  dev_ = 1;

  factories_["memfs"] = new TypedFsFactory<MemFs>;
  factories_["dev"] = new TypedFsFactory<DevFs>;
  factories_["googledrivefs"] = new TypedFsFactory<GoogleDriveFs>;
  factories_["html5fs"] = new TypedFsFactory<Html5Fs>;
  factories_["httpfs"] = new TypedFsFactory<HttpFs>;
  factories_["passthroughfs"] = new TypedFsFactory<PassthroughFs>;

  ScopedFilesystem root_fs;
  rtn = MountInternal("", "/", "passthroughfs", 0, NULL, false, &root_fs);
  if (rtn != 0)
    return rtn;

  ScopedFilesystem fs;
  rtn = MountInternal("", "/dev", "dev", 0, NULL, false, &fs);
  if (rtn != 0)
    return rtn;
  dev_fs_ = sdk_util::static_scoped_ref_cast<DevFs>(fs);

  // Create the filesystem nodes for / and /dev afterward. They can't be
  // created the normal way because the dev filesystem didn't exist yet.
  rtn = CreateFsNode(root_fs);
  if (rtn != 0)
    return rtn;

  rtn = CreateFsNode(dev_fs_);
  if (rtn != 0)
    return rtn;

  // Open the first three in order to get STDIN, STDOUT, STDERR
  int fd;
  fd = open("/dev/stdin", O_RDONLY, 0);
  if (fd < 0) {
    LOG_ERROR("failed to open /dev/stdin: %s", strerror(errno));
    return errno;
  }
  assert(fd == 0);

  fd = open("/dev/stdout", O_WRONLY, 0);
  if (fd < 0) {
    LOG_ERROR("failed to open /dev/stdout: %s", strerror(errno));
    return errno;
  }
  assert(fd == 1);

  fd = open("/dev/stderr", O_WRONLY, 0);
  if (fd < 0) {
    LOG_ERROR("failed to open /dev/sterr: %s", strerror(errno));
    return errno;
  }
  assert(fd == 2);

#ifdef PROVIDES_SOCKET_API
  host_resolver_.Init(ppapi_);
#endif

  FsInitArgs args;
  args.dev = dev_++;
  args.ppapi = ppapi_;
  stream_fs_.reset(new StreamFs());
  int result = stream_fs_->Init(args);
  if (result != 0) {
    LOG_ERROR("initializing streamfs failed: %s", strerror(result));
    return result;
  }

  return 0;
}

bool KernelProxy::RegisterFsType(const char* fs_type,
                                 fuse_operations* fuse_ops) {
  FsFactoryMap_t::iterator iter = factories_.find(fs_type);
  if (iter != factories_.end())
    return false;

  factories_[fs_type] = new FuseFsFactory(fuse_ops);
  return true;
}

bool KernelProxy::UnregisterFsType(const char* fs_type) {
  FsFactoryMap_t::iterator iter = factories_.find(fs_type);
  if (iter == factories_.end())
    return false;

  delete iter->second;
  factories_.erase(iter);
  return true;
}

void KernelProxy::SetExitCallback(nacl_io_exit_callback_t exit_callback,
                                  void* user_data) {
  exit_callback_ = exit_callback;
  exit_callback_user_data_ = user_data;
}

void KernelProxy::SetMountCallback(nacl_io_mount_callback_t mount_callback,
                                   void* user_data) {
  mount_callback_ = mount_callback;
  mount_callback_user_data_ = user_data;
}

int KernelProxy::open_resource(const char* path) {
  ScopedFilesystem fs;
  Path rel;

  Error error = AcquireFsAndRelPath(path, &fs, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  ScopedNode node;
  error = fs->OpenResource(rel, &node);
  if (error) {
    // OpenResource failed, try Open().
    error = fs->Open(rel, O_RDONLY, &node);
    if (error) {
      errno = error;
      return -1;
    }
  }

  ScopedKernelHandle handle(new KernelHandle(fs, node));
  error = handle->Init(O_RDONLY);
  if (error) {
    errno = error;
    return -1;
  }

  return AllocateFD(handle, path);
}

int KernelProxy::open(const char* path, int open_flags, mode_t mode) {
  ScopedFilesystem fs;
  ScopedNode node;
  mode_t mask = ~GetUmask() & S_MODEBITS;

  Error error = AcquireFsAndNode(path, open_flags, mode & mask, &fs, &node);
  if (error) {
    errno = error;
    return -1;
  }

  ScopedKernelHandle handle(new KernelHandle(fs, node));
  error = handle->Init(open_flags);
  if (error) {
    errno = error;
    return -1;
  }

  return AllocateFD(handle, path);
}

int KernelProxy::pipe(int pipefds[2]) {
  PipeNode* pipe = new PipeNode(stream_fs_.get());
  ScopedNode node(pipe);

  if (pipe->Init(O_RDWR) == 0) {
    ScopedKernelHandle handle0(new KernelHandle(stream_fs_, node));
    ScopedKernelHandle handle1(new KernelHandle(stream_fs_, node));

    // Should never fail, but...
    if (handle0->Init(O_RDONLY) || handle1->Init(O_WRONLY)) {
      errno = EACCES;
      return -1;
    }

    pipefds[0] = AllocateFD(handle0);
    pipefds[1] = AllocateFD(handle1);
    return 0;
  }

  errno = ENOSYS;
  return -1;
}

int KernelProxy::close(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  // Remove the FD from the process open file descriptor map
  FreeFD(fd);
  return 0;
}

int KernelProxy::dup(int oldfd) {
  ScopedKernelHandle handle;
  std::string path;
  Error error = AcquireHandleAndPath(oldfd, &handle, &path);
  if (error) {
    errno = error;
    return -1;
  }
  return AllocateFD(handle, path);
}

int KernelProxy::dup2(int oldfd, int newfd) {
  // If it's the same file handle, just return
  if (oldfd == newfd)
    return newfd;

  if (newfd < 0) {
    errno = EBADF;
    return -1;
  }

  ScopedKernelHandle old_handle;
  std::string old_path;
  Error error = AcquireHandleAndPath(oldfd, &old_handle, &old_path);
  if (error) {
    errno = error;
    return -1;
  }

  FreeAndReassignFD(newfd, old_handle, old_path);
  return newfd;
}

int KernelProxy::chdir(const char* path) {
  Error error = SetCWD(path);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

void KernelProxy::exit(int status) {
  if (exit_callback_)
    exit_callback_(status, exit_callback_user_data_);
}

char* KernelProxy::getcwd(char* buf, size_t size) {
  if (NULL == buf) {
    errno = EFAULT;
    return NULL;
  }

  std::string cwd = GetCWD();

  // Verify the buffer is large enough
  if (size <= cwd.size()) {
    errno = ERANGE;
    return NULL;
  }

  strcpy(buf, cwd.c_str());
  return buf;
}

char* KernelProxy::getwd(char* buf) {
  if (NULL == buf) {
    errno = EFAULT;
    return NULL;
  }
  return getcwd(buf, MAXPATHLEN);
}

int KernelProxy::chmod(const char* path, mode_t mode) {
  ScopedFilesystem fs;
  ScopedNode node;

  Error error = AcquireFsAndNode(path, O_RDONLY, 0, &fs, &node);
  if (error) {
    errno = error;
    return -1;
  }

  error = node->Fchmod(mode & S_MODEBITS);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::chown(const char* path, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::fchown(int fd, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::lchown(const char* path, uid_t owner, gid_t group) {
  return 0;
}

int KernelProxy::mkdir(const char* path, mode_t mode) {
  ScopedFilesystem fs;
  Path rel;

  Error error = AcquireFsAndRelPath(path, &fs, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  mode_t mask = ~GetUmask() & S_MODEBITS;
  error = fs->Mkdir(rel, mode & mask);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::rmdir(const char* path) {
  ScopedFilesystem fs;
  Path rel;

  Error error = AcquireFsAndRelPath(path, &fs, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = fs->Rmdir(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::stat(const char* path, struct stat* buf) {
  ScopedFilesystem fs;
  ScopedNode node;

  Error error = AcquireFsAndNode(path, O_RDONLY, 0, &fs, &node);
  if (error) {
    errno = error;
    return -1;
  }

  error = node->GetStat(buf);
  if (error) {
    errno = error;
    return -1;
  }

  /*
   * newlib's scandir() assumes that directories are empty if st_size == 0.
   * This is probably a bad assumption, but until we fix newlib always return
   * a non-zero directory size.
   */
  if (node->IsaDir() && buf->st_size == 0)
    buf->st_size = 4096;

  return 0;
}

int KernelProxy::mount(const char* source,
                       const char* target,
                       const char* filesystemtype,
                       unsigned long mountflags,
                       const void* data) {
  ScopedFilesystem fs;
  Error error = MountInternal(
      source, target, filesystemtype, mountflags, data, true, &fs);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

Error KernelProxy::MountInternal(const char* source,
                                 const char* target,
                                 const char* filesystemtype,
                                 unsigned long mountflags,
                                 const void* data,
                                 bool create_fs_node,
                                 ScopedFilesystem* out_filesystem) {
  std::string abs_path = GetAbsParts(target).Join();

  // Find a factory of that type
  FsFactoryMap_t::iterator factory = factories_.find(filesystemtype);
  if (factory == factories_.end()) {
    LOG_ERROR("Unknown filesystem type: %s", filesystemtype);
    return ENODEV;
  }

  // Create a map of settings
  StringMap_t smap;
  smap["SOURCE"] = source;

  if (data) {
    std::vector<std::string> elements;
    sdk_util::SplitString(static_cast<const char*>(data), ',', &elements);

    for (std::vector<std::string>::const_iterator it = elements.begin();
         it != elements.end();
         ++it) {
      size_t location = it->find('=');
      if (location != std::string::npos) {
        std::string key = it->substr(0, location);
        std::string val = it->substr(location + 1);
        smap[key] = val;
      } else {
        smap[*it] = "TRUE";
      }
    }
  }

  FsInitArgs args;
  args.dev = dev_++;
  args.string_map = smap;
  args.ppapi = ppapi_;

  ScopedFilesystem fs;
  Error error = factory->second->CreateFilesystem(args, &fs);
  if (error)
    return error;

  error = AttachFsAtPath(fs, abs_path);
  if (error)
    return error;

  if (create_fs_node) {
    error = CreateFsNode(fs);
    if (error) {
      DetachFsAtPath(abs_path, &fs);
      return error;
    }
  }

  *out_filesystem = fs;

  if (mount_callback_) {
    mount_callback_(source,
                    target,
                    filesystemtype,
                    mountflags,
                    data,
                    fs->dev(),
                    mount_callback_user_data_);
  }

  return 0;
}

Error KernelProxy::CreateFsNode(const ScopedFilesystem& fs) {
  assert(dev_fs_);

  return dev_fs_->CreateFsNode(fs.get());
}

int KernelProxy::umount(const char* path) {
  ScopedFilesystem fs;
  Error error = DetachFsAtPath(path, &fs);
  if (error) {
    errno = error;
    return -1;
  }

  error = dev_fs_->DestroyFsNode(fs.get());
  if (error) {
    // Ignore any errors here, just log.
    LOG_ERROR("Unable to destroy FsNode: %s", strerror(error));
  }
  return 0;
}

ssize_t KernelProxy::read(int fd, void* buf, size_t nbytes) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int cnt = 0;
  error = handle->Read(buf, nbytes, &cnt);
  if (error) {
    errno = error;
    return -1;
  }

  return cnt;
}

ssize_t KernelProxy::write(int fd, const void* buf, size_t nbytes) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int cnt = 0;
  error = handle->Write(buf, nbytes, &cnt);
  if (error) {
    errno = error;
    return -1;
  }

  return cnt;
}

int KernelProxy::fstat(int fd, struct stat* buf) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->GetStat(buf);
  if (error) {
    errno = error;
    return -1;
  }

  /*
   * newlib's scandir() assumes that directories are empty if st_size == 0.
   * This is probably a bad assumption, but until we fix newlib always return
   * a non-zero directory size.
   */
  if (handle->node()->IsaDir() && buf->st_size == 0)
    buf->st_size = 4096;

  return 0;
}

int KernelProxy::getdents(int fd, struct dirent* buf, unsigned int count) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int cnt = 0;
  error = handle->GetDents(buf, count, &cnt);
  if (error)
    errno = error;

  return cnt;
}

int KernelProxy::fchdir(int fd) {
  ScopedKernelHandle handle;
  std::string path;
  Error error = AcquireHandleAndPath(fd, &handle, &path);
  if (error) {
    errno = error;
    return -1;
  }

  if (!handle->node()->IsaDir()) {
    errno = ENOTDIR;
    return -1;
  }

  if (path.empty()) {
    errno = EBADF;
    return -1;
  }

  error = SetCWD(path);
  if (error) {
    // errno is return value from SetCWD
    errno = error;
    return -1;
  }
  return 0;
}

int KernelProxy::ftruncate(int fd, off_t length) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  if (handle->OpenMode() == O_RDONLY) {
    errno = EACCES;
    return -1;
  }

  error = handle->node()->FTruncate(length);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::fsync(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->FSync();
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::fdatasync(int fd) {
  errno = ENOSYS;
  return -1;
}

int KernelProxy::isatty(int fd) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return 0;
  }

  error = handle->node()->Isatty();
  if (error) {
    errno = error;
    return 0;
  }

  return 1;
}

int KernelProxy::ioctl(int fd, int request, va_list args) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->VIoctl(request, args);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::futimens(int fd, const struct timespec times[2]) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  return FutimensInternal(handle->node(), times);
}

Error KernelProxy::FutimensInternal(const ScopedNode& node,
                                    const struct timespec times[2]) {
  Error error(0);
  if (times == NULL) {
    struct timespec now[2];
    struct timeval tm;
    error = gettimeofday(&tm, NULL);
    if (error) {
      errno = error;
      return -1;
    }

    now[0].tv_sec = now[1].tv_sec = tm.tv_sec;
    now[0].tv_nsec = now[1].tv_nsec = tm.tv_usec * 1000;
    error = node->Futimens(now);
  } else {
    error = node->Futimens(times);
  }

  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

off_t KernelProxy::lseek(int fd, off_t offset, int whence) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  off_t new_offset;
  error = handle->Seek(offset, whence, &new_offset);
  if (error) {
    errno = error;
    return -1;
  }

  return new_offset;
}

int KernelProxy::unlink(const char* path) {
  ScopedFilesystem fs;
  Path rel;

  Error error = AcquireFsAndRelPath(path, &fs, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = fs->Unlink(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::truncate(const char* path, off_t len) {
  ScopedFilesystem fs;
  ScopedNode node;

  Error error = AcquireFsAndNode(path, O_WRONLY, 0, &fs, &node);
  if (error) {
    errno = error;
    return -1;
  }

  // Directories cannot be truncated.
  if (node->IsaDir()) {
    return EISDIR;
  }

  if (!node->CanOpen(O_WRONLY)) {
    errno = EACCES;
    return -1;
  }

  error = node->FTruncate(len);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::lstat(const char* path, struct stat* buf) {
  return stat(path, buf);
}

int KernelProxy::rename(const char* path, const char* newpath) {
  ScopedFilesystem fs;
  Path rel;
  Error error = AcquireFsAndRelPath(path, &fs, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  ScopedFilesystem newfs;
  Path newrel;
  error = AcquireFsAndRelPath(newpath, &newfs, &newrel);
  if (error) {
    errno = error;
    return -1;
  }

  if (newfs.get() != fs.get()) {
    // Renaming across mountpoints is not allowed
    errno = EXDEV;
    return -1;
  }

  // They already point to the same path
  if (rel == newrel)
    return 0;

  error = fs->Rename(rel, newrel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::remove(const char* path) {
  ScopedFilesystem fs;
  Path rel;

  Error error = AcquireFsAndRelPath(path, &fs, &rel);
  if (error) {
    errno = error;
    return -1;
  }

  error = fs->Remove(rel);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::fchmod(int fd, mode_t mode) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Fchmod(mode & S_MODEBITS);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::fcntl(int fd, int request, va_list args) {
  Error error = 0;

  // F_GETFD and F_SETFD are descriptor specific flags that
  // are stored in the KernelObject's decriptor map unlike
  // F_GETFL and F_SETFL which are handle specific.
  switch (request) {
    case F_GETFD: {
      int rtn = -1;
      error = GetFDFlags(fd, &rtn);
      if (error) {
        errno = error;
        return -1;
      }
      return rtn;
    }
    case F_SETFD: {
      int flags = va_arg(args, int);
      error = SetFDFlags(fd, flags);
      if (error) {
        errno = error;
        return -1;
      }
      return 0;
    }
  }

  ScopedKernelHandle handle;
  error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int rtn = 0;
  error = handle->VFcntl(request, &rtn, args);
  if (error) {
    errno = error;
    return -1;
  }

  return rtn;
}

int KernelProxy::access(const char* path, int amode) {
  struct stat buf;
  int rtn = stat(path, &buf);
  if (rtn != 0)
    return rtn;

  if (((amode & R_OK) && !(buf.st_mode & S_IREAD)) ||
      ((amode & W_OK) && !(buf.st_mode & S_IWRITE)) ||
      ((amode & X_OK) && !(buf.st_mode & S_IEXEC))) {
    errno = EACCES;
    return -1;
  }

  return 0;
}

int KernelProxy::readlink(const char* path, char* buf, size_t count) {
  LOG_TRACE("readlink is not implemented.");
  errno = EINVAL;
  return -1;
}

int KernelProxy::utimens(const char* path, const struct timespec times[2]) {
  ScopedFilesystem fs;
  ScopedNode node;

  Error error = AcquireFsAndNode(path, O_WRONLY, 0, &fs, &node);
  if (error) {
    errno = error;
    return -1;
  }

  return FutimensInternal(node, times);
}

// TODO(bradnelson): Needs implementation.
int KernelProxy::link(const char* oldpath, const char* newpath) {
  LOG_TRACE("link is not implemented.");
  errno = EINVAL;
  return -1;
}

int KernelProxy::symlink(const char* oldpath, const char* newpath) {
  LOG_TRACE("symlink is not implemented.");
  errno = EINVAL;
  return -1;
}

void* KernelProxy::mmap(void* addr,
                        size_t length,
                        int prot,
                        int flags,
                        int fd,
                        size_t offset) {
  // We shouldn't be getting anonymous mmaps here.
  assert((flags & MAP_ANONYMOUS) == 0);
  assert(fd != -1);

  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return MAP_FAILED;
  }

  void* new_addr;
  error = handle->node()->MMap(addr, length, prot, flags, offset, &new_addr);
  if (error) {
    errno = error;
    return MAP_FAILED;
  }

  return new_addr;
}

int KernelProxy::munmap(void* addr, size_t length) {
  // NOTE: The comment below is from a previous discarded implementation that
  // tracks mmap'd regions. For simplicity, we no longer do this; because we
  // "snapshot" the contents of the file in mmap(), and don't support
  // write-back or updating the mapped region when the file is written, holding
  // on to the KernelHandle is pointless.
  //
  // If we ever do, these threading issues should be considered.

  //
  // WARNING: this function may be called by free().
  //
  // There is a potential deadlock scenario:
  // Thread 1: open() -> takes lock1 -> free() -> takes lock2
  // Thread 2: free() -> takes lock2 -> munmap() -> takes lock1
  //
  // Note that open() above could be any function that takes a lock that is
  // shared with munmap (this includes munmap!)
  //
  // To prevent this, we avoid taking locks in munmap() that are used by other
  // nacl_io functions that may call free. Specifically, we only take the
  // mmap_lock, which is only shared with mmap() above. There is still a
  // possibility of deadlock if mmap() or munmap() calls free(), so this is not
  // allowed.
  //
  // Unfortunately, munmap still needs to acquire other locks; see the call to
  // ReleaseHandle below which takes the process lock. This is safe as long as
  // this is never executed from free() -- we can be reasonably sure this is
  // true, because malloc only makes anonymous mmap() requests, and should only
  // be munmapping those allocations. We never add to mmap_info_list_ for
  // anonymous maps, so the unmap_list should always be empty when called from
  // free().
  return 0;
}

int KernelProxy::tcflush(int fd, int queue_selector) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Tcflush(queue_selector);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::tcgetattr(int fd, struct termios* termios_p) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Tcgetattr(termios_p);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::tcsetattr(int fd,
                           int optional_actions,
                           const struct termios* termios_p) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->node()->Tcsetattr(optional_actions, termios_p);
  if (error) {
    errno = error;
    return -1;
  }

  return 0;
}

int KernelProxy::kill(pid_t pid, int sig) {
  // Currently we don't even pretend that other processes exist
  // so we can only send a signal to outselves.  For kill(2)
  // pid 0 means the current process group and -1 means all the
  // processes we have permission to send signals to.
  if (pid != getpid() && pid != -1 && pid != 0) {
    errno = ESRCH;
    return -1;
  }

  // Raise an event so that select/poll get interrupted.
  AUTO_LOCK(signal_emitter_->GetLock())
  signal_emitter_->RaiseEvents_Locked(POLLERR);
  switch (sig) {
    case SIGWINCH:
      if (sigwinch_handler_.sa_handler != SIG_IGN &&
          sigwinch_handler_.sa_handler != SIG_DFL) {
        sigwinch_handler_.sa_handler(SIGWINCH);
      }
      break;

    case SIGUSR1:
    case SIGUSR2:
      break;

    default:
      LOG_TRACE("Unsupported signal: %d", sig);
      errno = EINVAL;
      return -1;
  }
  return 0;
}

int KernelProxy::sigaction(int signum,
                           const struct sigaction* action,
                           struct sigaction* oaction) {
#if defined(SA_SIGINFO)
  if (action && action->sa_flags & SA_SIGINFO) {
    // We don't support SA_SIGINFO (sa_sigaction field) yet
    errno = EINVAL;
    return -1;
  }
#endif

  switch (signum) {
    // Handled signals.
    case SIGWINCH: {
      if (oaction)
        *oaction = sigwinch_handler_;
      if (action) {
        sigwinch_handler_ = *action;
      }
      return 0;
    }

    // Known signals
    case SIGHUP:
    case SIGINT:
    case SIGPIPE:
#if defined(SIGPOLL)
    case SIGPOLL:
#endif
    case SIGPROF:
    case SIGTERM:
    case SIGCHLD:
    case SIGURG:
    case SIGFPE:
    case SIGILL:
    case SIGQUIT:
    case SIGSEGV:
    case SIGTRAP:
      if (action && action->sa_handler != SIG_DFL) {
        // Trying to set this action to anything other than SIG_DFL
        // is not yet supported.
        LOG_TRACE("sigaction on signal %d != SIG_DFL not supported.", signum);
        errno = EINVAL;
        return -1;
      }

      if (oaction) {
        memset(oaction, 0, sizeof(*oaction));
        oaction->sa_handler = SIG_DFL;
      }
      return 0;

    // KILL and STOP cannot be handled
    case SIGKILL:
    case SIGSTOP:
      LOG_TRACE("sigaction on SIGKILL/SIGSTOP not supported.");
      errno = EINVAL;
      return -1;
  }

  // Unknown signum
  errno = EINVAL;
  return -1;
}

mode_t KernelProxy::umask(mode_t mask) {
  return SetUmask(mask & S_MODEBITS);
}

#ifdef PROVIDES_SOCKET_API

int KernelProxy::select(int nfds,
                        fd_set* readfds,
                        fd_set* writefds,
                        fd_set* exceptfds,
                        struct timeval* timeout) {
  std::vector<pollfd> pollfds;

  for (int fd = 0; fd < nfds; fd++) {
    int events = 0;
    if (readfds && FD_ISSET(fd, readfds)) {
      events |= POLLIN;
      FD_CLR(fd, readfds);
    }

    if (writefds && FD_ISSET(fd, writefds)) {
      events |= POLLOUT;
      FD_CLR(fd, writefds);
    }

    if (exceptfds && FD_ISSET(fd, exceptfds)) {
      events |= POLLERR | POLLHUP;
      FD_CLR(fd, exceptfds);
    }

    if (events) {
      pollfd info;
      info.fd = fd;
      info.events = events;
      pollfds.push_back(info);
    }
  }

  // NULL timeout signals wait forever.
  int ms_timeout = -1;
  if (timeout != NULL) {
    int64_t ms = timeout->tv_sec * 1000 + ((timeout->tv_usec + 500) / 1000);

    // If the timeout is invalid or too long (larger than signed 32 bit).
    if ((timeout->tv_sec < 0) || (timeout->tv_sec >= (INT_MAX / 1000)) ||
        (timeout->tv_usec < 0) || (timeout->tv_usec >= 1000000) || (ms < 0) ||
        (ms >= INT_MAX)) {
      LOG_TRACE("Invalid timeout: tv_sec=%" PRIi64 " tv_usec=%ld.",
                timeout->tv_sec,
                timeout->tv_usec);
      errno = EINVAL;
      return -1;
    }

    ms_timeout = static_cast<int>(ms);
  }

  int result = poll(&pollfds[0], pollfds.size(), ms_timeout);
  if (result == -1)
    return -1;

  int event_cnt = 0;
  for (size_t index = 0; index < pollfds.size(); index++) {
    pollfd* info = &pollfds[index];
    if (info->revents & POLLIN) {
      FD_SET(info->fd, readfds);
      event_cnt++;
    }
    if (info->revents & POLLOUT) {
      FD_SET(info->fd, writefds);
      event_cnt++;
    }
    if (info->revents & (POLLHUP | POLLERR)) {
      FD_SET(info->fd, exceptfds);
      event_cnt++;
    }
  }

  return event_cnt;
}

struct PollInfo {
  PollInfo() : index(-1) {}

  std::vector<struct pollfd*> fds;
  int index;
};

typedef std::map<EventEmitter*, PollInfo> EventPollMap_t;

int KernelProxy::poll(struct pollfd* fds, nfds_t nfds, int timeout) {
  EventPollMap_t event_map;

  std::vector<EventRequest> requests;
  size_t event_cnt = 0;

  for (int index = 0; static_cast<nfds_t>(index) < nfds; index++) {
    ScopedKernelHandle handle;
    struct pollfd* fd_info = &fds[index];
    Error err = AcquireHandle(fd_info->fd, &handle);

    fd_info->revents = 0;

    // If the node isn't open, or somehow invalid, mark it so.
    if (err != 0) {
      fd_info->revents = POLLNVAL;
      event_cnt++;
      continue;
    }

    // If it's already signaled, then just capture the event
    ScopedEventEmitter emitter(handle->node()->GetEventEmitter());
    int events = POLLIN | POLLOUT;
    if (emitter)
      events = emitter->GetEventStatus();

    if (events & fd_info->events) {
      fd_info->revents = events & fd_info->events;
      event_cnt++;
      continue;
    }

    if (NULL == emitter) {
      fd_info->revents = POLLNVAL;
      event_cnt++;
      continue;
    }

    // Otherwise try to track it.
    PollInfo* info = &event_map[emitter.get()];
    if (info->index == -1) {
      EventRequest request;
      request.emitter = emitter;
      request.filter = fd_info->events;
      request.events = 0;

      info->index = requests.size();
      requests.push_back(request);
    }
    info->fds.push_back(fd_info);
    requests[info->index].filter |= fd_info->events;
  }

  // If nothing is signaled, then we must wait on the event map
  if (0 == event_cnt) {
    EventListenerPoll wait;
    Error err = wait.WaitOnAny(&requests[0], requests.size(), timeout);
    if ((err != 0) && (err != ETIMEDOUT)) {
      errno = err;
      return -1;
    }

    for (size_t rindex = 0; rindex < requests.size(); rindex++) {
      EventRequest* request = &requests[rindex];
      if (request->events) {
        PollInfo* poll_info = &event_map[request->emitter.get()];
        for (size_t findex = 0; findex < poll_info->fds.size(); findex++) {
          struct pollfd* fd_info = poll_info->fds[findex];
          uint32_t events = fd_info->events & request->events;
          if (events) {
            fd_info->revents = events;
            event_cnt++;
          }
        }
      }
    }
  }

  return event_cnt;
}

// Socket Functions
int KernelProxy::accept(int fd, struct sockaddr* addr, socklen_t* len) {
  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  PP_Resource new_sock = 0;
  error = handle->Accept(&new_sock, addr, len);
  if (error != 0) {
    errno = error;
    return -1;
  }

  SocketNode* sock = new TcpNode(stream_fs_.get(), new_sock);

  // The SocketNode now holds a reference to the new socket
  // so we release ours.
  ppapi_->ReleaseResource(new_sock);
  error = sock->Init(O_RDWR);
  if (error != 0) {
    errno = error;
    return -1;
  }

  ScopedNode node(sock);
  ScopedKernelHandle new_handle(new KernelHandle(stream_fs_, node));
  error = new_handle->Init(O_RDWR);
  if (error != 0) {
    errno = error;
    return -1;
  }

  return AllocateFD(new_handle);
}

int KernelProxy::bind(int fd, const struct sockaddr* addr, socklen_t len) {
  if (NULL == addr) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->Bind(addr, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::connect(int fd, const struct sockaddr* addr, socklen_t len) {
  if (NULL == addr) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  error = handle->Connect(addr, len);
  if (error != 0) {
    errno = error;
    return -1;
  }

  return 0;
}

void KernelProxy::freeaddrinfo(struct addrinfo* res) {
  return host_resolver_.freeaddrinfo(res);
}

int KernelProxy::getaddrinfo(const char* node,
                             const char* service,
                             const struct addrinfo* hints,
                             struct addrinfo** res) {
  return host_resolver_.getaddrinfo(node, service, hints, res);
}

int KernelProxy::getnameinfo(const struct sockaddr *sa,
                             socklen_t salen,
                             char *host,
                             size_t hostlen,
                             char *serv,
                             size_t servlen,
                             int flags) {
  return host_resolver_.getnameinfo(sa, salen, host, hostlen, serv, servlen,
                                    flags);
}

struct hostent* KernelProxy::gethostbyname(const char* name) {
  return host_resolver_.gethostbyname(name);
}

int KernelProxy::getpeername(int fd, struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->GetPeerName(addr, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::getsockname(int fd, struct sockaddr* addr, socklen_t* len) {
  if (NULL == addr || NULL == len) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->GetSockName(addr, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::getsockopt(int fd,
                            int lvl,
                            int optname,
                            void* optval,
                            socklen_t* len) {
  if (NULL == optval || NULL == len) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->GetSockOpt(lvl, optname, optval, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::listen(int fd, int backlog) {
  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->Listen(backlog);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

ssize_t KernelProxy::recv(int fd, void* buf, size_t len, int flags) {
  if (NULL == buf) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int out_len = 0;
  error = handle->Recv(buf, len, flags, &out_len);
  if (error != 0) {
    errno = error;
    return -1;
  }

  return static_cast<ssize_t>(out_len);
}

ssize_t KernelProxy::recvfrom(int fd,
                              void* buf,
                              size_t len,
                              int flags,
                              struct sockaddr* addr,
                              socklen_t* addrlen) {
  // According to the manpage, recvfrom with a null addr is identical to recv.
  if (NULL == addr) {
    return recv(fd, buf, len, flags);
  }

  if (NULL == buf || NULL == addrlen) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int out_len = 0;
  error = handle->RecvFrom(buf, len, flags, addr, addrlen, &out_len);
  if (error != 0) {
    errno = error;
    return -1;
  }

  return static_cast<ssize_t>(out_len);
}

ssize_t KernelProxy::recvmsg(int fd, struct msghdr* msg, int flags) {
  if (NULL == msg) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  int total_len = 0;
  int out_len = 0;
  for (size_t i = 0; i < static_cast<size_t>(msg->msg_iovlen); i++) {
    if (NULL == msg->msg_iov[i].iov_base) {
      errno = EFAULT;
      return -1;
    }
    // Note that msg_control is not implemented.
    Error error = handle->RecvFrom(msg->msg_iov[i].iov_base,
                                   msg->msg_iov[i].iov_len, flags,
                                   static_cast<struct sockaddr*>(msg->msg_name),
                                   &msg->msg_namelen, &out_len);
    if (error != 0) {
      errno = error;
      return -1;
    }
    total_len += out_len;
  }

  return static_cast<ssize_t>(total_len);
}

ssize_t KernelProxy::send(int fd, const void* buf, size_t len, int flags) {
  if (NULL == buf) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int out_len = 0;
  error = handle->Send(buf, len, flags, &out_len);
  if (error != 0) {
    errno = error;
    return -1;
  }

  return static_cast<ssize_t>(out_len);
}

ssize_t KernelProxy::sendto(int fd,
                            const void* buf,
                            size_t len,
                            int flags,
                            const struct sockaddr* addr,
                            socklen_t addrlen) {
  // According to the manpage, sendto with a null addr is identical to send.
  if (NULL == addr) {
    return send(fd, buf, len, flags);
  }

  if (NULL == buf) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  Error error = AcquireHandle(fd, &handle);
  if (error) {
    errno = error;
    return -1;
  }

  int out_len = 0;
  error = handle->SendTo(buf, len, flags, addr, addrlen, &out_len);
  if (error != 0) {
    errno = error;
    return -1;
  }

  return static_cast<ssize_t>(out_len);
}

ssize_t KernelProxy::sendmsg(int fd, const struct msghdr* msg, int flags) {
  if (NULL == msg) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  int total_len = 0;
  int out_len = 0;
  for (size_t i = 0; i < static_cast<size_t>(msg->msg_iovlen); i++) {
    if (NULL == msg->msg_iov[i].iov_base) {
      errno = EFAULT;
      return -1;
    }
    // Note that msg_control is not implemented.
    Error error = handle->SendTo(msg->msg_iov[i].iov_base,
                                 msg->msg_iov[i].iov_len, flags,
                                 static_cast<struct sockaddr*>(msg->msg_name),
                                 msg->msg_namelen, &out_len);
    if (error != 0) {
      errno = error;
      return -1;
    }
    total_len += out_len;
  }

  return static_cast<ssize_t>(total_len);
}

int KernelProxy::setsockopt(int fd,
                            int lvl,
                            int optname,
                            const void* optval,
                            socklen_t len) {
  if (NULL == optval) {
    errno = EFAULT;
    return -1;
  }

  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->SetSockOpt(lvl, optname, optval, len);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::shutdown(int fd, int how) {
  ScopedKernelHandle handle;
  if (AcquireSocketHandle(fd, &handle) == -1)
    return -1;

  Error err = handle->socket_node()->Shutdown(how);
  if (err != 0) {
    errno = err;
    return -1;
  }

  return 0;
}

int KernelProxy::socket(int domain, int type, int protocol) {
  if (AF_INET != domain && AF_INET6 != domain) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  int open_flags = O_RDWR;

#if defined(SOCK_CLOEXEC)
  if (type & SOCK_CLOEXEC) {
#if defined(O_CLOEXEC)
    // The NaCl newlib version of fcntl.h doesn't currently define
    // O_CLOEXEC.
    // TODO(sbc): remove this guard once it gets added.
    open_flags |= O_CLOEXEC;
#endif
    type &= ~SOCK_CLOEXEC;
  }
#endif

#if defined(SOCK_NONBLOCK)
  if (type & SOCK_NONBLOCK) {
    open_flags |= O_NONBLOCK;
    type &= ~SOCK_NONBLOCK;
  }
#endif

  SocketNode* sock = NULL;
  switch (type) {
    case SOCK_DGRAM:
      sock = new UdpNode(stream_fs_.get());
      break;

    case SOCK_STREAM:
      sock = new TcpNode(stream_fs_.get());
      break;

    case SOCK_SEQPACKET:
    case SOCK_RDM:
    case SOCK_RAW:
      errno = EPROTONOSUPPORT;
      return -1;

    default:
      errno = EINVAL;
      return -1;
  }

  ScopedNode node(sock);
  Error rtn = sock->Init(O_RDWR);
  if (rtn != 0) {
    errno = rtn;
    return -1;
  }

  ScopedKernelHandle handle(new KernelHandle(stream_fs_, node));
  rtn = handle->Init(open_flags);
  if (rtn != 0) {
    errno = rtn;
    return -1;
  }

  return AllocateFD(handle);
}

int KernelProxy::socketpair(int domain, int type, int protocol, int* sv) {
  if (NULL == sv) {
    errno = EFAULT;
    return -1;
  }

  if (AF_INET == domain || AF_INET6 == domain) {
    errno = EOPNOTSUPP;
    return -1;
  }

  if (AF_UNIX != domain) {
    errno = EAFNOSUPPORT;
    return -1;
  }

  // TODO(cernekee): mask this off with SOCK_TYPE_MASK first.
  if (SOCK_STREAM != type && SOCK_DGRAM != type) {
    errno = EPROTOTYPE;
    return -1;
  }

  int open_flags = O_RDWR;

#if defined(SOCK_CLOEXEC)
  if (type & SOCK_CLOEXEC) {
#if defined(O_CLOEXEC)
    // The NaCl newlib version of fcntl.h doesn't currently define
    // O_CLOEXEC.
    // TODO(sbc): remove this guard once it gets added.
    open_flags |= O_CLOEXEC;
#endif
    type &= ~SOCK_CLOEXEC;
  }
#endif

#if defined(SOCK_NONBLOCK)
  if (type & SOCK_NONBLOCK) {
    open_flags |= O_NONBLOCK;
    type &= ~SOCK_NONBLOCK;
  }
#endif

  UnixNode* socket = new UnixNode(stream_fs_.get(), type);
  Error rtn = socket->Init(O_RDWR);
  if (rtn != 0) {
    errno = rtn;
    return -1;
  }
  ScopedNode node0(socket);
  socket = new UnixNode(stream_fs_.get(), *socket);
  rtn = socket->Init(O_RDWR);
  if (rtn != 0) {
    errno = rtn;
    return -1;
  }
  ScopedNode node1(socket);
  ScopedKernelHandle handle0(new KernelHandle(stream_fs_, node0));
  rtn = handle0->Init(open_flags);
  if (rtn != 0) {
    errno = rtn;
    return -1;
  }
  ScopedKernelHandle handle1(new KernelHandle(stream_fs_, node1));
  rtn = handle1->Init(open_flags);
  if (rtn != 0) {
    errno = rtn;
    return -1;
  }

  sv[0] = AllocateFD(handle0);
  sv[1] = AllocateFD(handle1);

  return 0;
}

int KernelProxy::AcquireSocketHandle(int fd, ScopedKernelHandle* handle) {
  Error error = AcquireHandle(fd, handle);

  if (error) {
    errno = error;
    return -1;
  }

  if ((handle->get()->node_->GetType() & S_IFSOCK) == 0) {
    errno = ENOTSOCK;
    return -1;
  }

  return 0;
}

#endif  // PROVIDES_SOCKET_API

}  // namespace_nacl_io
