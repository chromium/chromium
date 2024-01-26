/*
 * Copyright (C) 2019 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"

namespace blink {

MemoryManagedPaintRecorder::MemoryManagedPaintRecorder(gfx::Size size,
                                                       Client* client)
    : client_(client), canvas_(size) {
  if (client_) {
    client_->InitializeForRecording(&canvas_);
  }
}

MemoryManagedPaintRecorder::~MemoryManagedPaintRecorder() = default;

void MemoryManagedPaintRecorder::SetClient(Client* client) {
  client_ = client;
}

cc::PaintRecord MemoryManagedPaintRecorder::finishRecordingAsPicture() {
  cc::PaintRecord record = canvas_.ReleaseAsRecord();
  if (client_) {
    client_->InitializeForRecording(&canvas_);
  }
  return record;
}

void MemoryManagedPaintRecorder::SkipQueuedDrawCommands() {
  // If no draw calls have been recorded, we have nothing to skip. The recoding
  // could still contain layers or matrix clip stack levels. As an optimization,
  // we can keep the recording untouched as there is no need to discard the
  // layer matrix clip stack just to rebuild it again.
  if (HasRecordedDrawOps()) {
    finishRecordingAsPicture();
  }

  if (client_) {
    client_->RecordingCleared();
  }
}

void MemoryManagedPaintRecorder::RestartRecording() {
  // Discard the whole recording and re-initialize it.
  finishRecordingAsPicture();
  if (client_) {
    client_->RecordingCleared();
  }
}

}  // namespace blink
