// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_SOCKET_UDP_NODE_H_
#define LIBRARIES_NACL_IO_SOCKET_UDP_NODE_H_

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <ppapi/c/pp_resource.h>
#include <ppapi/c/ppb_udp_socket.h>

#include "nacl_io/socket/socket_node.h"
#include "nacl_io/socket/udp_event_emitter.h"

namespace nacl_io {

class UdpNode : public SocketNode {
 public:
  explicit UdpNode(Filesystem* filesystem);

 protected:
  virtual Error Init(int open_flags);
  virtual void Destroy();

 public:
  virtual UdpEventEmitter* GetEventEmitter();

  virtual void QueueInput();
  virtual void QueueOutput();

  virtual Error Bind(const struct sockaddr* addr, socklen_t len);
  virtual Error Connect(const HandleAttr& attr,
                        const struct sockaddr* addr,
                        socklen_t len);

 protected:
  virtual Error Recv_Locked(void* buf,
                            size_t len,
                            PP_Resource* addr,
                            int* out_len);

  virtual Error Send_Locked(const void* buf,
                            size_t len,
                            PP_Resource addr,
                            int* out_len);

  virtual Error SetSockOptSocket(int opname, const void* optval, socklen_t len);

  virtual Error SetSockOptIP(int optname, const void* optval, socklen_t len);

  virtual Error SetSockOptIPV6(int optname, const void* optval, socklen_t len);

 protected:
  ScopedUdpEventEmitter emitter_;
};

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
#endif  // LIBRARIES_NACL_IO_SOCKET_UDP_NODE_H_
