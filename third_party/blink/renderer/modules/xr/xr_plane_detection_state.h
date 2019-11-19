// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_DETECTION_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_DETECTION_STATE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class XRPlaneDetectionStateInit;

class XRPlaneDetectionState : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRPlaneDetectionState(
      XRPlaneDetectionStateInit* plane_detection_state_init = nullptr);

  bool enabled() const { return enabled_; }

 private:
  bool enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_PLANE_DETECTION_STATE_H_
