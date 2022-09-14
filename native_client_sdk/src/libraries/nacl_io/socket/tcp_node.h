// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_SOCKET_TCP_NODE_H_
#define LIBRARIES_NACL_IO_SOCKET_TCP_NODE_H_

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <ppapi/c/pp_resource.h>
#include <ppapi/c/ppb_tcp_socket.h>

#include "nacl_io/node.h"
#include "nacl_io/socket/socket_node.h"
#include "nacl_io/socket/tcp_event_emitter.h"

namespace nacl_io {

class TcpNode : public SocketNode {
 public:
  explicit TcpNode(Filesystem* filesystem);
  TcpNode(Filesystem* filesystem, PP_Resource socket);

 protected:
  virtual Error Init(int open_flags);
  virtual void Destroy();

 public:
  virtual EventEmitter* GetEventEmitter();

  virtual void QueueAccept();
  virtual void QueueConnect();
  virtual void QueueInput();
  virtual void QueueOutput();

  virtual Error Accept(const HandleAttr& attr,
                       PP_Resource* out_sock,
                       struct sockaddr* addr,
                       socklen_t* len);
  virtual Error Bind(const struct sockaddr* addr, socklen_t len);
  virtual Error Connect(const HandleAttr& attr,
                        const struct sockaddr* addr,
                        socklen_t len);
  virtual Error GetSockOpt(int lvl, int optname, void* optval, socklen_t* len);
  virtual Error Listen(int backlog);
  virtual Error Shutdown(int how);

  virtual void SetError_Locked(int pp_error_num);
  void ConnectDone_Locked();
  void ConnectFailed_Locked();

 protected:
  Error SetNoDelay_Locked();
  virtual Error Recv_Locked(void* buf,
                            size_t len,
                            PP_Resource* out_addr,
                            int* out_len);

  virtual Error Send_Locked(const void* buf,
                            size_t len,
                            PP_Resource addr,
                            int* out_len);

  virtual Error SetSockOptSocket(int opname, const void* optval, socklen_t len);

  virtual Error SetSockOptTCP(int optname, const void* optval, socklen_t len);

 protected:
  ScopedTcpEventEmitter emitter_;
  PP_Resource accepted_socket_;
  bool tcp_nodelay_;
};

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
#endif  // LIBRARIES_NACL_IO_SOCKET_TCP_NODE_H_
