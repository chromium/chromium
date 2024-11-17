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

#include "base/types/optional_ref.h"

namespace blink {

MemoryManagedPaintRecorder::MemoryManagedPaintRecorder(gfx::Size size,
                                                       Client* client)
    : client_(client), size_(size), main_canvas_(size) {
  if (client_) {
    client_->InitializeForRecording(&main_canvas_);
  }
}

MemoryManagedPaintRecorder::~MemoryManagedPaintRecorder() = default;

void MemoryManagedPaintRecorder::SetClient(Client* client) {
  client_ = client;
}

void MemoryManagedPaintRecorder::DisableLineDrawingAsPaths() {
  main_canvas_.DisableLineDrawingAsPaths();
  if (side_canvas_) {
    side_canvas_->DisableLineDrawingAsPaths();
  }
}

cc::PaintRecord MemoryManagedPaintRecorder::ReleaseMainRecording() {
  cc::PaintRecord record = main_canvas_.ReleaseAsRecord();
  // ReleaseAsRecord() clears the paint ops, so we need initialize the recording
  // for subsequent draw calls.
  if (client_) {
    client_->InitializeForRecording(&main_canvas_);
  }
  return record;
}

cc::PaintRecord MemoryManagedPaintRecorder::CopyMainRecording() {
  // CopyAsRecord() does not clear the paint ops, so we do not need to call
  // InitializeForRecording().
  return main_canvas_.CopyAsRecord();
}

void MemoryManagedPaintRecorder::RestartCurrentLayer() {
  if (HasSideRecording()) {
    // We are recording in the side canvas, which groups together all layers
    // into a single recording. We therefore do not know where the child-most
    // layer starts in this side recording and therefore cannot drop it.
    // This could be improved by keeping a stack of canvas, one per layers.
    return;
  }

  // If no draw calls have been recorded, we have nothing to skip. The recoding
  // could still contain layers or matrix clip stack levels. As an optimization,
  // we can keep the recording untouched as there is no need to discard the
  // layer matrix clip stack just to rebuild it again.
  if (HasRecordedDrawOps()) {
    ReleaseMainRecording();
  }

  if (client_) {
    client_->RecordingCleared();
  }
}

void MemoryManagedPaintRecorder::RestartRecording() {
  current_canvas_ = &main_canvas_;
  side_canvas_ = nullptr;
  ReleaseMainRecording();
  if (client_) {
    client_->RecordingCleared();
  }
}

void MemoryManagedPaintRecorder::BeginSideRecording() {
  CHECK(!side_canvas_) << "BeginSideRecording() can't be called when side "
                          "recording is already active.";
  side_canvas_ = main_canvas_.CreateChildCanvas();
  current_canvas_ = side_canvas_.get();
}

void MemoryManagedPaintRecorder::EndSideRecording() {
  CHECK(side_canvas_) << "EndSideRecording() can't be called without "
                         "first calling BeginSideRecording().";
  main_canvas_.drawPicture(side_canvas_->ReleaseAsRecord(),
                           /*local_ctm=*/false);
  current_canvas_ = &main_canvas_;
  side_canvas_ = nullptr;
}

}  // namespace blink
