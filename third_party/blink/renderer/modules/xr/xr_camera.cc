// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_camera.h"

#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRCamera::XRCamera(XRFrame* frame)
    : frame_(frame), size_(*(frame_->session()->CameraImageSize())) {}

XRFrame* XRCamera::Frame() const {
  return frame_.Get();
}

void XRCamera::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
