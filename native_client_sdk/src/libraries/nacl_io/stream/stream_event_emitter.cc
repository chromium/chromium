// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/stream/stream_event_emitter.h"

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>

#include "nacl_io/fifo_interface.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

StreamEventEmitter::StreamEventEmitter() : stream_(NULL) {
}

void StreamEventEmitter::AttachStream(StreamNode* stream) {
  AUTO_LOCK(GetLock());
  stream_ = stream;
}

void StreamEventEmitter::DetachStream() {
  AUTO_LOCK(GetLock());

  RaiseEvents_Locked(POLLHUP);
  stream_ = NULL;
}

void StreamEventEmitter::UpdateStatus_Locked() {
  uint32_t status = 0;
  if (!in_fifo()->IsEmpty())
    status |= POLLIN;

  if (!out_fifo()->IsFull())
    status |= POLLOUT;

  ClearEvents_Locked(~status);
  RaiseEvents_Locked(status);
}

}  // namespace nacl_io
