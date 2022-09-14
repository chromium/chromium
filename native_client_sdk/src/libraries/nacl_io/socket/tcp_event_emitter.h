// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_SOCKET_TCP_EVENT_EMITTER_H_
#define LIBRARIES_NACL_IO_SOCKET_TCP_EVENT_EMITTER_H_

#include "nacl_io/fifo_char.h"
#include "nacl_io/stream/stream_event_emitter.h"

#include <ppapi/c/pp_resource.h>

#include "sdk_util/macros.h"
#include "sdk_util/scoped_ref.h"

namespace nacl_io {

class TcpEventEmitter;

typedef sdk_util::ScopedRef<TcpEventEmitter> ScopedTcpEventEmitter;

class TcpEventEmitter : public StreamEventEmitter {
 public:
  TcpEventEmitter(size_t rsize, size_t wsize);

  TcpEventEmitter(const TcpEventEmitter&) = delete;
  TcpEventEmitter& operator=(const TcpEventEmitter&) = delete;

  uint32_t ReadIn_Locked(char* buffer, uint32_t len);
  uint32_t WriteIn_Locked(const char* buffer, uint32_t len);

  uint32_t ReadOut_Locked(char* buffer, uint32_t len);
  uint32_t WriteOut_Locked(const char* buffer, uint32_t len);

  bool GetError_Locked();
  void SetError_Locked();
  void ConnectDone_Locked();
  PP_Resource GetAcceptedSocket_Locked();
  void SetAcceptedSocket_Locked(PP_Resource socket);
  void UpdateStatus_Locked();
  void SetListening_Locked();
  void SetRecvEndOfStream_Locked();

  uint32_t BytesInOutputFIFO();
  uint32_t SpaceInInputFIFO();

 protected:
  virtual FIFOChar* in_fifo() { return &in_fifo_; }
  virtual FIFOChar* out_fifo() { return &out_fifo_; }

 private:
  FIFOChar in_fifo_;
  FIFOChar out_fifo_;
  bool error_;
  bool listening_;
  bool recv_endofstream_;
  PP_Resource accepted_socket_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_SOCKET_TCP_EVENT_EMITTER_H_
