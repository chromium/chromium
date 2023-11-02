// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/kernel_handle.h"

#include <errno.h>
#include <pthread.h>

#include "nacl_io/filesystem.h"
#include "nacl_io/node.h"
#include "nacl_io/osunistd.h"
#include "nacl_io/socket/socket_node.h"

#include "sdk_util/auto_lock.h"

namespace nacl_io {

// It is only legal to construct a handle while the kernel lock is held.
KernelHandle::KernelHandle() : filesystem_(NULL), node_(NULL) {
}

KernelHandle::KernelHandle(const ScopedFilesystem& fs, const ScopedNode& node)
    : filesystem_(fs), node_(node) {
}

KernelHandle::~KernelHandle() {
  // Force release order for cases where filesystem_ is not ref'd by mounting.
  node_.reset(NULL);
  filesystem_.reset(NULL);
}

// Returns the SocketNode* if this node is a socket.
SocketNode* KernelHandle::socket_node() {
  if (node_.get() && node_->IsaSock())
    return reinterpret_cast<SocketNode*>(node_.get());
  return NULL;
}

Error KernelHandle::Init(int open_flags) {
  handle_attr_.flags = open_flags;

  if ((open_flags & O_CREAT) == 0 && !node_->CanOpen(open_flags)) {
    return EACCES;
  }

  // Directories can only be opened read-only.
  if ((open_flags & 3) != O_RDONLY && node_->IsaDir()) {
    return EISDIR;
  }

  if (open_flags & O_APPEND) {
    Error error = node_->GetSize(&handle_attr_.offs);
    if (error)
      return error;
  }

  return 0;
}

Error KernelHandle::Seek(off_t offset, int whence, off_t* out_offset) {
  // By default, don't move the offset.
  *out_offset = offset;
  off_t base;
  off_t node_size;

  AUTO_LOCK(handle_lock_);
  if (!node_->IsSeekable())
    return ESPIPE;

  Error error = node_->GetSize(&node_size);
  if (error)
    return error;

  switch (whence) {
    case SEEK_SET:
      base = 0;
      break;
    case SEEK_CUR:
      base = handle_attr_.offs;
      break;
    case SEEK_END:
      base = node_size;
      break;
    default:
      return -1;
  }

  if (base + offset < 0)
    return EINVAL;

  off_t new_offset = base + offset;

  // Seeking past the end of the file will zero out the space between the old
  // end and the new end.
  if (new_offset > node_size) {
    error = node_->FTruncate(new_offset);
    if (error)
      return EINVAL;
  }

  *out_offset = handle_attr_.offs = new_offset;
  return 0;
}

Error KernelHandle::Read(void* buf, size_t nbytes, int* cnt) {
  sdk_util::AutoLock read_lock(handle_lock_);
  if (OpenMode() == O_WRONLY)
    return EACCES;
  if (!node_->IsSeekable()){
    read_lock.Unlock();
    AUTO_LOCK(input_lock_);
    return node_->Read(handle_attr_, buf, nbytes, cnt);
  }
  Error error = node_->Read(handle_attr_, buf, nbytes, cnt);
  if (0 == error)
    handle_attr_.offs += *cnt;
  return error;
}

Error KernelHandle::Write(const void* buf, size_t nbytes, int* cnt) {
  sdk_util::AutoLock write_lock(handle_lock_);
  if (OpenMode() == O_RDONLY)
    return EACCES;
  if (!node_->IsSeekable()){
    write_lock.Unlock();
    AUTO_LOCK(output_lock_);
    return node_->Write(handle_attr_, buf, nbytes, cnt);
  }
  Error error = node_->Write(handle_attr_, buf, nbytes, cnt);
  if (0 == error)
    handle_attr_.offs += *cnt;
  return error;
}

Error KernelHandle::GetDents(struct dirent* pdir, size_t nbytes, int* cnt) {
  AUTO_LOCK(handle_lock_);
  Error error = node_->GetDents(handle_attr_.offs, pdir, nbytes, cnt);
  if (0 == error)
    handle_attr_.offs += *cnt;
  return error;
}

Error KernelHandle::Fcntl(int request, int* result, ...) {
  va_list ap;
  va_start(ap, result);
  Error rtn = VFcntl(request, result, ap);
  va_end(ap);
  return rtn;
}

Error KernelHandle::VFcntl(int request, int* result, va_list args) {
  switch (request) {
    case F_GETFL: {
      // Should not block, but could if blocked on Connect or Accept. This is
      // acceptable.
      AUTO_LOCK(handle_lock_);
      *result = handle_attr_.flags;
      return 0;
    }
    case F_SETFL: {
      AUTO_LOCK(handle_lock_);
      int flags = va_arg(args, int);
      if (!(flags & O_APPEND) && (handle_attr_.flags & O_APPEND)) {
        // Attempt to clear O_APPEND.
        return EPERM;
      }
      // Only certain flags are mutable
      const int mutable_flags = O_ASYNC | O_NONBLOCK;
      flags &= mutable_flags;
      handle_attr_.flags &= ~mutable_flags;
      handle_attr_.flags |= flags;
      return 0;
    }
    default:
      LOG_ERROR("Unsupported fcntl: %#x", request);
      break;
  }
  return ENOSYS;
}

Error KernelHandle::Accept(PP_Resource* new_sock,
                           struct sockaddr* addr,
                           socklen_t* len) {
  SocketNode* sock = socket_node();
  if (!sock)
    return ENOTSOCK;

  AUTO_LOCK(handle_lock_);
  return sock->Accept(handle_attr_, new_sock, addr, len);
}

Error KernelHandle::Connect(const struct sockaddr* addr, socklen_t len) {
  SocketNode* sock = socket_node();
  if (!sock)
    return ENOTSOCK;

  AUTO_LOCK(handle_lock_);
  return sock->Connect(handle_attr_, addr, len);
}

Error KernelHandle::Recv(void* buf, size_t len, int flags, int* out_len) {
  SocketNode* sock = socket_node();
  if (!sock)
    return ENOTSOCK;
  if (OpenMode() == O_WRONLY)
    return EACCES;

  AUTO_LOCK(input_lock_);
  return sock->Recv(handle_attr_, buf, len, flags, out_len);
}

Error KernelHandle::RecvFrom(void* buf,
                             size_t len,
                             int flags,
                             struct sockaddr* src_addr,
                             socklen_t* addrlen,
                             int* out_len) {
  SocketNode* sock = socket_node();
  if (!sock)
    return ENOTSOCK;
  if (OpenMode() == O_WRONLY)
    return EACCES;

  AUTO_LOCK(input_lock_);
  return sock->RecvFrom(handle_attr_, buf, len, flags, src_addr, addrlen,
                        out_len);
}

Error KernelHandle::Send(const void* buf, size_t len, int flags, int* out_len) {
  SocketNode* sock = socket_node();
  if (!sock)
    return ENOTSOCK;
  if (OpenMode() == O_RDONLY)
    return EACCES;

  AUTO_LOCK(output_lock_);
  return sock->Send(handle_attr_, buf, len, flags, out_len);
}

Error KernelHandle::SendTo(const void* buf,
                           size_t len,
                           int flags,
                           const struct sockaddr* dest_addr,
                           socklen_t addrlen,
                           int* out_len) {
  SocketNode* sock = socket_node();
  if (!sock)
    return ENOTSOCK;
  if (OpenMode() == O_RDONLY)
    return EACCES;

  AUTO_LOCK(output_lock_);
  return sock->SendTo(handle_attr_, buf, len, flags, dest_addr, addrlen,
                      out_len);
}

}  // namespace nacl_io
