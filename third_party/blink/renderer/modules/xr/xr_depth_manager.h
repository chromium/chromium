// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_MANAGER_H_

#include "base/types/pass_key.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class XRDepthInformation;
class XRFrame;
class XRSession;

// Helper class, used to separate the code related to depth buffer processing
// out of XRSession.
class XRDepthManager : public GarbageCollected<XRDepthManager> {
 public:
  explicit XRDepthManager(base::PassKey<XRSession> pass_key,
                          XRSession* session);
  virtual ~XRDepthManager();

  void ProcessDepthInformation(device::mojom::blink::XRDepthDataPtr depth_data);

  XRDepthInformation* GetDepthInformation(const XRFrame* xr_frame);

  void Trace(Visitor* visitor) const;

 private:
  Member<XRSession> session_;

  // Current depth data buffer.
  device::mojom::blink::XRDepthDataUpdatedPtr depth_data_;

  // Cached version of the depth buffer data. If not null, contains the same
  // information as |depth_data_.pixel_data| buffer.
  Member<DOMUint16Array> data_;

  void EnsureData();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DEPTH_MANAGER_H_
