// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_KERNEL_HANDLE_H_
#define LIBRARIES_NACL_IO_KERNEL_HANDLE_H_

#include <fcntl.h>
#include <pthread.h>
#include <ppapi/c/pp_resource.h>

#include "nacl_io/error.h"
#include "nacl_io/filesystem.h"
#include "nacl_io/node.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostypes.h"

#include "sdk_util/macros.h"
#include "sdk_util/ref_object.h"
#include "sdk_util/scoped_ref.h"
#include "sdk_util/simple_lock.h"

namespace nacl_io {

class SocketNode;

// HandleAttr struct is passed the Node in calls
// to Read and Write.  It contains handle specific state
// such as the file offset and the open flags.
struct HandleAttr {
  HandleAttr() : offs(0), flags(0) {}
  bool IsBlocking() const { return !(flags & O_NONBLOCK); }

  off_t offs;
  int flags;
};

// KernelHandle provides a reference counted container for the open
// file information, such as it's filesystem, node, access type and offset.
// KernelHandle can only be referenced when the KernelProxy lock is held.
class KernelHandle : public sdk_util::RefObject {
 public:
  KernelHandle();
  KernelHandle(const ScopedFilesystem& fs, const ScopedNode& node);

  KernelHandle(const KernelHandle&) = delete;
  KernelHandle& operator=(const KernelHandle&) = delete;

  ~KernelHandle();

  Error Init(int open_flags);

  Error Accept(PP_Resource* new_sock, struct sockaddr* addr, socklen_t* len);
  Error Connect(const struct sockaddr* addr, socklen_t len);
  Error Fcntl(int request, int* result, ...);
  Error VFcntl(int request, int* result, va_list args);
  Error GetDents(struct dirent* pdir, size_t count, int* bytes_written);
  Error Read(void* buf, size_t nbytes, int* bytes_read);
  Error Recv(void* buf, size_t len, int flags, int* out_len);
  Error RecvFrom(void* buf,
                 size_t len,
                 int flags,
                 struct sockaddr* src_addr,
                 socklen_t* addrlen,
                 int* out_len);
  // Assumes |out_offset| is non-NULL.
  Error Seek(off_t offset, int whence, off_t* out_offset);
  Error Send(const void* buf, size_t len, int flags, int* out_len);
  Error SendTo(const void* buf,
               size_t len,
               int flags,
               const struct sockaddr* dest_addr,
               socklen_t addrlen,
               int* out_len);
  Error Write(const void* buf, size_t nbytes, int* bytes_written);

  const ScopedNode& node() { return node_; }
  const ScopedFilesystem& filesystem() { return filesystem_; }

  const HandleAttr& Attr() { return handle_attr_; }

  int OpenMode() { return handle_attr_.flags & 3; }

 private:
  // Returns the SocketNode* if this node is a socket otherwise returns
  // NULL.
  SocketNode* socket_node();

  ScopedFilesystem filesystem_;
  ScopedNode node_;
  sdk_util::SimpleLock input_lock_;
  sdk_util::SimpleLock output_lock_;
  // Protects the handle and attributes.
  sdk_util::SimpleLock handle_lock_;
  HandleAttr handle_attr_;

  friend class KernelProxy;
};

typedef sdk_util::ScopedRef<KernelHandle> ScopedKernelHandle;

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_KERNEL_HANDLE_H_
