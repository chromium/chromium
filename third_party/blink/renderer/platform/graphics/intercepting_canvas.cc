// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/intercepting_canvas.h"

namespace blink {

void InterceptingCanvasBase::UnrollDrawPicture(
    const SkPicture* picture,
    const SkMatrix* matrix,
    const SkPaint* paint,
    SkPicture::AbortCallback* abort_callback) {
  int save_count = getSaveCount();
  if (paint) {
    SkRect new_bounds = picture->cullRect();
    if (matrix)
      matrix->mapRect(&new_bounds);
    saveLayer(&new_bounds, paint);
  } else if (matrix) {
    save();
  }
  if (matrix)
    concat(*matrix);

  picture->playback(this, abort_callback);

  restoreToCount(save_count);
}

}  // namespace blink
