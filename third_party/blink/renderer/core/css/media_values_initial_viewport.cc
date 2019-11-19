// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_values_initial_viewport.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink {

MediaValuesInitialViewport::MediaValuesInitialViewport(LocalFrame& frame)
    : MediaValuesDynamic(&frame) {}

double MediaValuesInitialViewport::ViewportWidth() const {
  DCHECK(frame_->View());
  return frame_->View()->InitialViewportWidth();
}

double MediaValuesInitialViewport::ViewportHeight() const {
  DCHECK(frame_->View());
  return frame_->View()->InitialViewportHeight();
}

}  // namespace blink
