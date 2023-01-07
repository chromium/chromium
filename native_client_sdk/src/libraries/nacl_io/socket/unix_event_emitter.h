// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_SOCKET_UNIX_EVENT_EMITTER_H_
#define LIBRARIES_NACL_IO_SOCKET_UNIX_EVENT_EMITTER_H_

#include "nacl_io/fifo_interface.h"
#include "nacl_io/stream/stream_event_emitter.h"

#include "sdk_util/macros.h"
#include "sdk_util/scoped_ref.h"
#include "sdk_util/simple_lock.h"

namespace nacl_io {

class UnixEventEmitter;

typedef sdk_util::ScopedRef<UnixEventEmitter> ScopedUnixEventEmitter;

class UnixEventEmitter : public StreamEventEmitter {
 public:
  uint32_t ReadIn_Locked(char* buffer, uint32_t len);
  uint32_t WriteOut_Locked(const char* buffer, uint32_t len);

  uint32_t BytesInOutputFIFO();
  uint32_t SpaceInInputFIFO();

  virtual ScopedUnixEventEmitter GetPeerEmitter() = 0;
  virtual void Shutdown_Locked(bool read, bool write) = 0;
  virtual bool IsShutdownRead() const = 0;
  virtual bool IsShutdownWrite() const = 0;

  static ScopedUnixEventEmitter MakeUnixEventEmitter(size_t size, int type);

  UnixEventEmitter(const UnixEventEmitter&) = delete;
  UnixEventEmitter& operator=(const UnixEventEmitter&) = delete;

 protected:
  UnixEventEmitter() {}

  virtual FIFOInterface* in_fifo() = 0;
  virtual FIFOInterface* out_fifo() = 0;
  void UpdateStatus_Locked();
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_SOCKET_UNIX_EVENT_EMITTER_H_
