// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_MSFT_OPENXR_HIT_TEST_MANAGER_MSFT_H_
#define DEVICE_VR_OPENXR_MSFT_OPENXR_HIT_TEST_MANAGER_MSFT_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "device/vr/openxr/openxr_hit_test_manager.h"

namespace device {

class OpenXrPlaneManagerMsft;

class OpenXrHitTestManagerMsft : public OpenXrHitTestManager {
 public:
  explicit OpenXrHitTestManagerMsft(OpenXrPlaneManagerMsft* plane_manager,
                                    XrSpace mojo_space);
  ~OpenXrHitTestManagerMsft() override;

  std::vector<mojom::XRHitResultPtr> RequestHitTest(
      const gfx::Point3F& origin,
      const gfx::Vector3dF& direction) override;

 protected:
  bool OnNewHitTestSubscription() override;
  void OnAllHitTestSubscriptionsRemoved() override;
  void OnStartProcessingHitTests(XrTime predicted_display_time) override;
  std::optional<float> GetRayPlaneDistance(const gfx::Point3F& ray_origin,
                                           const gfx::Vector3dF& ray_vector,
                                           const gfx::Point3F& plane_origin,
                                           const gfx::Vector3dF& plane_normal);
  raw_ptr<OpenXrPlaneManagerMsft> plane_manager_;
  XrSpace mojo_space_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_MSFT_OPENXR_HIT_TEST_MANAGER_MSFT_H_
