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

MemoryManagedPaintRecorder::MemoryManagedPaintRecorder(
    MemoryManagedPaintCanvas::Client* client)
    : client_(client) {
  DCHECK(client);
}

MemoryManagedPaintRecorder::~MemoryManagedPaintRecorder() = default;

cc::PaintCanvas* MemoryManagedPaintRecorder::beginRecording(
    const gfx::Size& size) {
  DCHECK(!is_recording_);
  is_recording_ = true;

  if (!canvas_ || size != size_) {
    canvas_ = std::make_unique<MemoryManagedPaintCanvas>(size, client_);
  }
  size_ = size;
  return canvas_.get();
}

sk_sp<cc::PaintRecord> MemoryManagedPaintRecorder::finishRecordingAsPicture() {
  DCHECK(canvas_);
  DCHECK(is_recording_);
  is_recording_ = false;
  return canvas_->ReleaseAsRecord();
}

}  // namespace blink
