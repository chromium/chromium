// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_graphics_binding.h"

#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRGraphicsBinding::XRGraphicsBinding(XRSession* session) : session_(session) {
  session_->AddGraphicsBinding(this);
}

void XRGraphicsBinding::PreFinalize() {
  DLOG(ERROR) << __func__;
  session_->RemoveGraphicsBinding(this);
}

double XRGraphicsBinding::nativeProjectionScaleFactor() const {
  return session_->NativeFramebufferScale();
}

void XRGraphicsBinding::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
}

}  // namespace blink
