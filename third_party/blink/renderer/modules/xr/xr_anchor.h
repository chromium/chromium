// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_

#include <memory>

#include "base/optional.h"
#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRSession;
class XRSpace;

class XRAnchor : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRAnchor(uint64_t id, XRSession* session);

  XRAnchor(uint64_t id,
           XRSession* session,
           const device::mojom::blink::XRAnchorDataPtr& anchor_data,
           double timestamp);

  uint64_t id() const;

  XRSpace* anchorSpace() const;

  TransformationMatrix poseMatrix() const;

  double lastChangedTime(bool& is_null) const;

  void detach();

  void Update(const device::mojom::blink::XRAnchorDataPtr& anchor_data,
              double timestamp);

  void Trace(blink::Visitor* visitor) override;

 private:
  // AnchorData will only be present in an XRAnchor after the anchor was updated
  // for the first time (CreateAnchor returns a promise that will resolve to an
  // XRAnchor prior to first update of the anchor).
  struct AnchorData {
    // Anchor's pose in device (mojo) space.
    std::unique_ptr<TransformationMatrix> pose_matrix_;
    double last_changed_time_;

    AnchorData(const device::mojom::blink::XRAnchorDataPtr& anchor_data,
               double timestamp);
  };

  const uint64_t id_;

  Member<XRSession> session_;

  base::Optional<AnchorData> anchor_data_;

  // Cached anchor space - it will be created by `anchorSpace()` if it's not
  // set.
  mutable Member<XRSpace> anchor_space_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_ANCHOR_H_
