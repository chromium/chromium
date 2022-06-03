// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HAND_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HAND_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class XRInputSource;
class XRJointSpace;

class XRHand : public ScriptWrappable,
               public PairIterable<String, Member<XRJointSpace>> {
  DEFINE_WRAPPERTYPEINFO();

  static const unsigned kNumJoints =
      static_cast<unsigned>(device::mojom::blink::XRHandJoint::kMaxValue) + 1u;

 public:
  explicit XRHand(const device::mojom::blink::XRHandTrackingData* state,
                  XRInputSource* input_source);
  ~XRHand() override = default;

  size_t size() const { return joints_.size(); }

  XRJointSpace* get(const String& key);

  void updateFromHandTrackingData(
      const device::mojom::blink::XRHandTrackingData* state,
      XRInputSource* input_source);

  bool hasMissingPoses() const { return has_missing_poses_; }

  void Trace(Visitor*) const override;

 private:
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;

  HeapVector<Member<XRJointSpace>> joints_;
  bool has_missing_poses_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HAND_H_
