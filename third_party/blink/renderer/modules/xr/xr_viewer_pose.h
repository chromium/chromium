// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_POSE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_POSE_H_

#include "third_party/blink/renderer/modules/xr/xr_pose.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"

namespace blink {

class XRSession;
class XRView;

class XRViewerPose final : public XRPose {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRViewerPose(XRSession*, const TransformationMatrix&);
  ~XRViewerPose() override = default;

  const HeapVector<Member<XRView>>& views() const { return views_; }

  void Trace(blink::Visitor*) override;

 private:
  HeapVector<Member<XRView>> views_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEWER_POSE_H_
