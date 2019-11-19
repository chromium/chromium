// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_view.h"

#include "third_party/blink/renderer/modules/xr/xr_test_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

TEST(XRViewTest, UpdatePoseMatrix) {
  XRViewData view(XRView::kEyeLeft);

  TransformationMatrix head_from_eye;
  head_from_eye.Translate3d(-1.0, 2.0, 3.0);
  view.SetHeadFromEyeTransform(head_from_eye);

  DOMPointInit* position = MakePointForTest(1.0, -1.0, 4.0, 1.0);
  DOMPointInit* orientation =
      MakePointForTest(0.3701005885691383, -0.5678993882056005,
                       0.31680366148754113, 0.663438979322567);
  XRRigidTransform* initial_transform =
      MakeGarbageCollected<XRRigidTransform>(position, orientation);
  TransformationMatrix pose_matrix = initial_transform->TransformMatrix();

  view.UpdatePoseMatrix(pose_matrix);
  TransformationMatrix view_transform_matrix = view.Transform();
  const Vector<double> actual_matrix =
      GetMatrixDataForTest(view_transform_matrix);

  const Vector<double> expected_matrix{
      0.154251,  0.000000,  0.988032,  0.000000,  -0.840720, 0.525322,
      0.131253,  0.000000,  -0.519035, -0.850904, 0.081032,  0.000000,
      -2.392795, -2.502067, 3.517570,  1.000000};

  for (int i = 0; i < 16; ++i) {
    ASSERT_NEAR(actual_matrix[i], expected_matrix[i], kEpsilon);
  }
}

}  // namespace
}  // namespace blink
