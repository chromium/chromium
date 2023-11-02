// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_PIPE_PIPE_EVENT_EMITTER_H_
#define LIBRARIES_NACL_IO_PIPE_PIPE_EVENT_EMITTER_H_

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "nacl_io/fifo_char.h"
#include "nacl_io/stream/stream_event_emitter.h"

#include "sdk_util/auto_lock.h"
#include "sdk_util/macros.h"

namespace nacl_io {

class PipeEventEmitter;
typedef sdk_util::ScopedRef<PipeEventEmitter> ScopedPipeEventEmitter;

class PipeEventEmitter : public StreamEventEmitter {
 public:
  explicit PipeEventEmitter(size_t size);

  PipeEventEmitter(const PipeEventEmitter&) = delete;
  PipeEventEmitter& operator=(const PipeEventEmitter&) = delete;

  Error Read_Locked(char* data, size_t len, int* out_bytes);
  Error Write_Locked(const char* data, size_t len, int* out_bytes);

 protected:
  virtual FIFOChar* in_fifo() { return &fifo_; }
  virtual FIFOChar* out_fifo() { return &fifo_; }

 private:
  FIFOChar fifo_;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_PIPE_PIPE_EVENT_EMITTER_H_
