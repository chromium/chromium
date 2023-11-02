// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"
#ifdef PROVIDES_SOCKET_API

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <algorithm>

#include "nacl_io/socket/unix_event_emitter.h"
#include "nacl_io/socket/unix_node.h"

namespace nacl_io {

UnixNode::UnixNode(Filesystem* filesystem, int type)
    : SocketNode(SOCK_STREAM, filesystem),
      emitter_(UnixEventEmitter::MakeUnixEventEmitter(65536, type)) {
  emitter_->AttachStream(this);
}

UnixNode::UnixNode(Filesystem* filesystem, const UnixNode& peer)
    : SocketNode(SOCK_STREAM, filesystem),
      emitter_(peer.emitter_->GetPeerEmitter()) {
  emitter_->AttachStream(this);
}

EventEmitter* UnixNode::GetEventEmitter() {
  return emitter_.get();
}

Error UnixNode::Recv_Locked(void* buffer,
                            size_t len,
                            PP_Resource* out_addr,
                            int* out_len) {
  assert(emitter_.get());
  *out_len = emitter_->ReadIn_Locked(static_cast<char*>(buffer), len);
  *out_addr = 0;
  return 0;
}

Error UnixNode::Send_Locked(const void* buffer,
                            size_t len,
                            PP_Resource out_addr,
                            int* out_len) {
  assert(emitter_.get());
  if (emitter_->IsShutdownWrite()) {
    return EPIPE;
  }
  *out_len = emitter_->WriteOut_Locked(static_cast<const char*>(buffer), len);
  return 0;
}

Error UnixNode::RecvFrom(const HandleAttr& attr,
                         void* buf,
                         size_t len,
                         int flags,
                         struct sockaddr* src_addr,
                         socklen_t* addrlen,
                         int* out_len) {
  PP_Resource addr = 0;
  Error err = RecvHelper(attr, buf, len, flags, &addr, out_len);
  if (0 == err) {
    if (src_addr) {
      unsigned short family = AF_UNIX;
      memcpy(src_addr, &family,
             std::min(*addrlen, static_cast<socklen_t>(sizeof(family))));
      *addrlen = sizeof(family);
    }
  }

  return err;
}

Error UnixNode::Send(const HandleAttr& attr,
                     const void* buf,
                     size_t len,
                     int flags,
                     int* out_len) {
  PP_Resource addr = 0;
  return SendHelper(attr, buf, len, flags, addr, out_len);
}

Error UnixNode::SendTo(const HandleAttr& attr,
                       const void* buf,
                       size_t len,
                       int flags,
                       const struct sockaddr* dest_addr,
                       socklen_t addrlen,
                       int* out_len) {
  PP_Resource addr = 0;
  return SendHelper(attr, buf, len, flags, addr, out_len);
}

Error UnixNode::Shutdown(int how) {
  bool read;
  bool write;
  switch (how) {
    case SHUT_RDWR:
      read = write = true;
      break;
    case SHUT_RD:
      read = true;
      write = false;
      break;
    case SHUT_WR:
      read = false;
      write = true;
      break;
    default:
      return EINVAL;
  }
  AUTO_LOCK(emitter_->GetLock());
  emitter_->Shutdown_Locked(read, write);
  return 0;
}

}  // namespace nacl_io

#endif  // PROVIDES_SOCKET_API
