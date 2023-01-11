// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_ANCHOR_REQUEST_H_
#define DEVICE_VR_OPENXR_OPENXR_ANCHOR_REQUEST_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

using CreateAnchorCallback =
    base::OnceCallback<void(device::mojom::CreateAnchorResult,
                            uint64_t anchor_id)>;

class CreateAnchorRequest {
 public:
  const mojom::XRNativeOriginInformation& GetNativeOriginInformation() const;
  const gfx::Transform& GetNativeOriginFromAnchor() const;
  const base::TimeTicks& GetRequestStartTime() const;

  CreateAnchorCallback TakeCallback();

  CreateAnchorRequest(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform native_origin_from_anchor,
      CreateAnchorCallback callback);
  CreateAnchorRequest(CreateAnchorRequest&& other);
  ~CreateAnchorRequest();

 private:
  const mojom::XRNativeOriginInformation native_origin_information_;
  const gfx::Transform native_origin_from_anchor_;
  const base::TimeTicks request_start_time_;

  CreateAnchorCallback callback_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_ANCHOR_REQUEST_H_
