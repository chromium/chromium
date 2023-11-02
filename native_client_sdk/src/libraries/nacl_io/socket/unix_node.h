// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_SOCKET_UNIX_NODE_H_
#define LIBRARIES_NACL_IO_SOCKET_UNIX_NODE_H_

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include "nacl_io/node.h"
#include "nacl_io/socket/socket_node.h"
#include "nacl_io/socket/unix_event_emitter.h"

namespace nacl_io {

class UnixNode : public SocketNode {
 public:
  UnixNode(Filesystem* filesystem, int type);
  UnixNode(Filesystem* filesystem, const UnixNode& peer);

  virtual EventEmitter* GetEventEmitter();

 protected:
  virtual Error Recv_Locked(void* buffer,
                            size_t len,
                            PP_Resource* out_addr,
                            int* out_len);
  virtual Error Send_Locked(const void* buffer,
                            size_t len,
                            PP_Resource addr,
                            int* out_len);
  virtual Error RecvFrom(const HandleAttr& attr,
                         void* buf,
                         size_t len,
                         int flags,
                         struct sockaddr* src_addr,
                         socklen_t* addrlen,
                         int* out_len);
  virtual Error Send(const HandleAttr& attr,
                     const void* buf,
                     size_t len,
                     int flags,
                     int* out_len);
  virtual Error SendTo(const HandleAttr& attr,
                       const void* buf,
                       size_t len,
                       int flags,
                       const struct sockaddr* dest_addr,
                       socklen_t addrlen,
                       int* out_len);
  virtual Error Shutdown(int how);

 private:
  ScopedUnixEventEmitter emitter_;
};

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
#endif  // LIBRARIES_NACL_IO_SOCKET_UNIX_NODE_H_
