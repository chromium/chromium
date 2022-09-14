// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/socket/tcp_event_emitter.h"

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "nacl_io/fifo_char.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

TcpEventEmitter::TcpEventEmitter(size_t rsize, size_t wsize)
    : in_fifo_(rsize),
      out_fifo_(wsize),
      error_(false),
      listening_(false),
      recv_endofstream_(false),
      accepted_socket_(0) {
}

uint32_t TcpEventEmitter::ReadIn_Locked(char* data, uint32_t len) {
  uint32_t count = in_fifo_.Read(data, len);
  UpdateStatus_Locked();
  return count;
}

void TcpEventEmitter::UpdateStatus_Locked() {
  if (error_) {
    RaiseEvents_Locked(POLLIN | POLLOUT);
    return;
  }

  if (recv_endofstream_) {
    RaiseEvents_Locked(POLLIN);
    return;
  }

  if (listening_) {
    if (accepted_socket_)
      RaiseEvents_Locked(POLLIN);
    return;
  }

  StreamEventEmitter::UpdateStatus_Locked();
}

void TcpEventEmitter::SetListening_Locked() {
  listening_ = true;
  UpdateStatus_Locked();
}

uint32_t TcpEventEmitter::WriteIn_Locked(const char* data, uint32_t len) {
  uint32_t count = in_fifo_.Write(data, len);

  UpdateStatus_Locked();
  return count;
}

uint32_t TcpEventEmitter::ReadOut_Locked(char* data, uint32_t len) {
  uint32_t count = out_fifo_.Read(data, len);

  UpdateStatus_Locked();
  return count;
}

uint32_t TcpEventEmitter::WriteOut_Locked(const char* data, uint32_t len) {
  uint32_t count = out_fifo_.Write(data, len);

  UpdateStatus_Locked();
  return count;
}

void TcpEventEmitter::ConnectDone_Locked() {
  RaiseEvents_Locked(POLLOUT);
  UpdateStatus_Locked();
}

bool TcpEventEmitter::GetError_Locked() {
  return error_;
}

void TcpEventEmitter::SetError_Locked() {
  error_ = true;
  UpdateStatus_Locked();
}

void TcpEventEmitter::SetAcceptedSocket_Locked(PP_Resource socket) {
  accepted_socket_ = socket;
  UpdateStatus_Locked();
}

PP_Resource TcpEventEmitter::GetAcceptedSocket_Locked() {
  int rtn = accepted_socket_;
  accepted_socket_ = 0;
  UpdateStatus_Locked();
  return rtn;
}

void TcpEventEmitter::SetRecvEndOfStream_Locked() {
  recv_endofstream_ = true;
  UpdateStatus_Locked();
}

uint32_t TcpEventEmitter::BytesInOutputFIFO() {
  return out_fifo()->ReadAvailable();
}

uint32_t TcpEventEmitter::SpaceInInputFIFO() {
  return in_fifo()->WriteAvailable();
}

}  // namespace nacl_io
