/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/replaying_canvas.h"

namespace blink {

CanvasInterceptor<ReplayingCanvas>::~CanvasInterceptor() {
  if (TopLevelCall())
    Canvas()->UpdateInRange();
}

ReplayingCanvas::ReplayingCanvas(SkBitmap bitmap,
                                 unsigned from_step,
                                 unsigned to_step)
    : InterceptingCanvas(bitmap),
      from_step_(from_step),
      to_step_(to_step),
      abort_drawing_(false) {}

void ReplayingCanvas::UpdateInRange() {
  if (abort_drawing_)
    return;
  unsigned step = CallCount() + 1;
  if (to_step_ && step > to_step_)
    abort_drawing_ = true;
  if (step == from_step_)
    SkCanvas::clear(SK_ColorTRANSPARENT);
}

bool ReplayingCanvas::abort() {
  return abort_drawing_;
}

SkCanvas::SaveLayerStrategy ReplayingCanvas::getSaveLayerStrategy(
    const SaveLayerRec& rec) {
  // We're about to create a layer and we have not cleared the device yet.
  // Let's clear now, so it has effect on all layers.
  if (CallCount() <= from_step_)
    SkCanvas::clear(SK_ColorTRANSPARENT);

  return InterceptingCanvas<ReplayingCanvas>::getSaveLayerStrategy(rec);
}

void ReplayingCanvas::onDrawPicture(const SkPicture* picture,
                                    const SkMatrix* matrix,
                                    const SkPaint* paint) {
  UnrollDrawPicture(picture, matrix, paint, this);
}

}  // namespace blink
