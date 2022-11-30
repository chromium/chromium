// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/pipe/pipe_event_emitter.h"

#include <algorithm>

namespace nacl_io {

PipeEventEmitter::PipeEventEmitter(size_t size)
    : fifo_(std::max<size_t>(1, size)) {
  UpdateStatus_Locked();
}

Error PipeEventEmitter::Read_Locked(char* data, size_t len, int* out_bytes) {
  *out_bytes = fifo_.Read(data, len);

  UpdateStatus_Locked();
  return 0;
}

Error PipeEventEmitter::Write_Locked(const char* data,
                                     size_t len,
                                     int* out_bytes) {
  *out_bytes = fifo_.Write(data, len);

  UpdateStatus_Locked();
  return 0;
}

}  // namespace nacl_io
