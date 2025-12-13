// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_HIT_TEST_MANAGER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_HIT_TEST_MANAGER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_hit_test_manager.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrPlaneManagerAndroid;
class OpenXrExtensionHelper;

class OpenXrHitTestManagerAndroid : public OpenXrHitTestManager {
 public:
  OpenXrHitTestManagerAndroid(OpenXrPlaneManagerAndroid* plane_manager,
                              const OpenXrExtensionHelper& extension_helper,
                              XrSession session,
                              XrSpace mojo_space);
  ~OpenXrHitTestManagerAndroid() override;

  std::vector<mojom::XRHitResultPtr> RequestHitTest(
      const gfx::Point3F& origin,
      const gfx::Vector3dF& direction) override;

 private:
  void OnStartProcessingHitTests(XrTime predicted_display_time) override;

  raw_ptr<OpenXrPlaneManagerAndroid> plane_manager_;
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrTime predicted_display_time_ = 0;
  XrSpace mojo_space_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_HIT_TEST_MANAGER_ANDROID_H_
