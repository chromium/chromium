// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/node.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <string>

#include "nacl_io/filesystem.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/osmman.h"
#include "nacl_io/ostime.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

static const int USR_ID = 1001;
static const int GRP_ID = 1002;

Node::Node(Filesystem* filesystem) : filesystem_(filesystem) {
  memset(&stat_, 0, sizeof(stat_));
  stat_.st_gid = GRP_ID;
  stat_.st_uid = USR_ID;
  stat_.st_mode = S_IRALL | S_IWALL;

  // Filesystem should normally never be NULL, but may be null in tests.
  // If NULL, at least set the inode to a valid (nonzero) value.
  if (filesystem_)
    filesystem_->OnNodeCreated(this);
  else
    stat_.st_ino = 1;
}

Node::~Node() {
}

Error Node::Init(int open_flags) {
  return 0;
}

void Node::Destroy() {
  if (filesystem_) {
    filesystem_->OnNodeDestroyed(this);
  }
}

EventEmitter* Node::GetEventEmitter() {
  return NULL;
}

uint32_t Node::GetEventStatus() {
  if (GetEventEmitter())
    return GetEventEmitter()->GetEventStatus();

  return POLLIN | POLLOUT;
}

bool Node::CanOpen(int open_flags) {
  switch (open_flags & 3) {
    case O_RDONLY:
      return (stat_.st_mode & S_IRALL) != 0;
    case O_WRONLY:
      return (stat_.st_mode & S_IWALL) != 0;
    case O_RDWR:
      return (stat_.st_mode & S_IRALL) != 0 && (stat_.st_mode & S_IWALL) != 0;
  }

  return false;
}

Error Node::FSync() {
  return 0;
}

Error Node::FTruncate(off_t length) {
  return EINVAL;
}

Error Node::GetDents(size_t offs,
                     struct dirent* pdir,
                     size_t count,
                     int* out_bytes) {
  *out_bytes = 0;
  return ENOTDIR;
}

Error Node::GetStat(struct stat* pstat) {
  AUTO_LOCK(node_lock_);
  memcpy(pstat, &stat_, sizeof(stat_));
  return 0;
}

Error Node::Ioctl(int request, ...) {
  va_list ap;
  va_start(ap, request);
  Error rtn = VIoctl(request, ap);
  va_end(ap);
  return rtn;
}

Error Node::VIoctl(int request, va_list args) {
  return EINVAL;
}

Error Node::Read(const HandleAttr& attr,
                 void* buf,
                 size_t count,
                 int* out_bytes) {
  *out_bytes = 0;
  return EINVAL;
}

Error Node::Write(const HandleAttr& attr,
                  const void* buf,
                  size_t count,
                  int* out_bytes) {
  *out_bytes = 0;
  return EINVAL;
}

Error Node::MMap(void* addr,
                 size_t length,
                 int prot,
                 int flags,
                 size_t offset,
                 void** out_addr) {
  *out_addr = NULL;

  // Never allow mmap'ing PROT_EXEC. The passthrough node supports this, but we
  // don't. Fortunately, glibc will fallback if this fails, so dlopen will
  // continue to work.
  if (prot & PROT_EXEC)
    return EPERM;

  // This default mmap support is just enough to make dlopen work.  This
  // implementation just reads from the filesystem into the mmap'd memory area.
  void* new_addr = addr;
  int mmap_error = _real_mmap(
      &new_addr, length, prot | PROT_WRITE, flags | MAP_ANONYMOUS, -1, 0);
  if (new_addr == MAP_FAILED) {
    _real_munmap(new_addr, length);
    return mmap_error;
  }

  HandleAttr data;
  data.offs = offset;
  data.flags = 0;
  int bytes_read;
  Error read_error = Read(data, new_addr, length, &bytes_read);
  if (read_error) {
    _real_munmap(new_addr, length);
    return read_error;
  }

  *out_addr = new_addr;
  return 0;
}

Error Node::Tcflush(int queue_selector) {
  return EINVAL;
}

Error Node::Tcgetattr(struct termios* termios_p) {
  return EINVAL;
}

Error Node::Tcsetattr(int optional_actions, const struct termios* termios_p) {
  return EINVAL;
}

Error Node::Futimens(const struct timespec times[2]) {
  return 0;
}

Error Node::Fchmod(mode_t mode) {
  return EINVAL;
}

int Node::GetLinks() {
  return stat_.st_nlink;
}

int Node::GetMode() {
  return stat_.st_mode & S_MODEBITS;
}

Error Node::GetSize(off_t* out_size) {
  *out_size = stat_.st_size;
  return 0;
}

int Node::GetType() {
  return stat_.st_mode & S_IFMT;
}

void Node::SetType(int type) {
  assert((type & ~S_IFMT) == 0);
  stat_.st_mode &= ~S_IFMT;
  stat_.st_mode |= type;
}

void Node::SetMode(int mode) {
  assert((mode & ~S_MODEBITS) == 0);
  stat_.st_mode &= ~S_MODEBITS;
  stat_.st_mode |= mode;
}

bool Node::IsaDir() {
  return GetType() == S_IFDIR;
}

bool Node::IsaFile() {
  return GetType() == S_IFREG;
}

bool Node::IsaSock() {
  return GetType() == S_IFSOCK;
}

bool Node::IsSeekable() {
  return !(IsaSock() || GetType() == S_IFIFO);
}

Error Node::Isatty() {
  return ENOTTY;
}

Error Node::AddChild(const std::string& name, const ScopedNode& node) {
  return ENOTDIR;
}

Error Node::RemoveChild(const std::string& name) {
  return ENOTDIR;
}

Error Node::FindChild(const std::string& name, ScopedNode* out_node) {
  out_node->reset(NULL);
  return ENOTDIR;
}

int Node::ChildCount() {
  return 0;
}

void Node::Link() {
  stat_.st_nlink++;
}

void Node::Unlink() {
  stat_.st_nlink--;
}

void Node::UpdateTime(int update_bits) {
  struct timeval now;
  gettimeofday(&now, NULL);

  // TODO(binji): honor noatime mount option?
  if (update_bits & UPDATE_ATIME) {
    stat_.st_atime = now.tv_sec;
    stat_.st_atimensec = now.tv_usec * 1000;
  }
  if (update_bits & UPDATE_MTIME) {
    stat_.st_mtime = now.tv_sec;
    stat_.st_mtimensec = now.tv_usec * 1000;
  }
  if (update_bits & UPDATE_CTIME) {
    stat_.st_ctime = now.tv_sec;
    stat_.st_ctimensec = now.tv_usec * 1000;
  }
}

}  // namespace nacl_io
