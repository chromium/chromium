// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_INFORMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_INFORMATION_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/xr_plane_set.h"

namespace blink {

class XRSession;

class XRWorldInformation : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRWorldInformation(XRSession* session);

  // Returns set of detected planes. Returns null if plane detection is
  // disabled.
  XRPlaneSet* detectedPlanes() const;

  void Trace(blink::Visitor* visitor) override;

  // Applies changes to the stored plane information based on the contents of
  // the received frame data. This will update the contents of
  // plane_ids_to_planes_.
  void ProcessPlaneInformation(
      const device::mojom::blink::XRPlaneDetectionDataPtr& detected_planes_data,
      double timestamp);

 private:
  // Signifies if we should return null from `detectedPlanes()`.
  // This is the case if we have a freshly constructed instance, or if our
  // last `ProcessPlaneInformation()` was called with base::nullopt.
  bool is_detected_planes_null_ = true;
  HeapHashMap<uint64_t, Member<XRPlane>> plane_ids_to_planes_;

  Member<XRSession> session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WORLD_INFORMATION_H_
