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

  using THandJointsMapKey = String;
  using THandJointsMapValue = Member<XRJointSpace>;
  using THandJointCollection =
      std::array<std::tuple<THandJointsMapKey, THandJointsMapValue>,
                 kNumJoints>;

 public:
  explicit XRHand(const device::mojom::blink::XRHandTrackingData* state,
                  XRInputSource* input_source);
  ~XRHand() override = default;

  unsigned int size() const { return joint_spaces_.size(); }

  XRJointSpace* get(const String& key);

  void Trace(Visitor*) const override;

 private:
  class HandJointIterationSource final
      : public PairIterable<THandJointsMapKey,
                            THandJointsMapValue>::IterationSource {
   public:
    explicit HandJointIterationSource(const THandJointCollection& joint_spaces);

    bool Next(ScriptState* script_state,
              THandJointsMapKey& key,
              THandJointsMapValue& value,
              ExceptionState& exception_state) override;

   private:
    const THandJointCollection& joint_spaces_;

    uint32_t current;
  };

  using Iterationsource =
      PairIterable<String, Member<XRJointSpace>>::IterationSource;
  IterationSource* StartIteration(ScriptState*, ExceptionState&) override {
    return MakeGarbageCollected<HandJointIterationSource>(joint_spaces_);
  }

  THandJointCollection joint_spaces_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_HAND_H_
