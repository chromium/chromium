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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_RECORDER_H_

#include "base/memory/raw_ptr.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT MemoryManagedPaintRecorder {
 public:
  class Client {
   public:
    virtual void InitializeForRecording(cc::PaintCanvas* canvas) const = 0;
    virtual void RecordingCleared() = 0;
  };

  // If specified, `client` is notified for events from this object. `client`
  // must outlive this `MemoryManagedPaintRecorder`.
  explicit MemoryManagedPaintRecorder(gfx::Size size, Client* client);
  ~MemoryManagedPaintRecorder();

  void SetClient(Client* client);

  cc::PaintRecord finishRecordingAsPicture();

  // Drops all draw ops from the recording while preserving the layer and matrix
  // clip stack. This is done by discarding the whole recording and rebuilding
  // the layer and matrix clip stack. If the recording contains no draw calls,
  // the flush and stack rebuild is optimized out.
  void SkipQueuedDrawCommands();

  // Restarts the whole recording. This will rebuild the layer and matrix clip
  // stack, but since this function is meant to be called after resetting the
  // canvas state stack, the matrix clip stack should be rebuilt to it's default
  // initial state.
  void RestartRecording();

  bool HasRecordedDrawOps() const { return canvas_.HasRecordedDrawOps(); }
  size_t TotalOpCount() const { return canvas_.TotalOpCount(); }
  size_t OpBytesUsed() const { return canvas_.OpBytesUsed(); }
  size_t ImageBytesUsed() const { return canvas_.ImageBytesUsed(); }

  cc::PaintCanvas* getRecordingCanvas() { return &canvas_; }

 private:
  // Pointer to the client interested in events from this
  // `MemoryManagedPaintRecorder`. If `nullptr`, notifications are disabled.
  raw_ptr<Client, ExperimentalRenderer> client_ = nullptr;

  MemoryManagedPaintCanvas canvas_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_RECORDER_H_
