// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_view.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/xr/xr_test_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {
namespace {

void AssertMatrixEquals(const Vector<double>& actual,
                        const Vector<double>& expected) {
  for (int i = 0; i < 16; ++i) {
    ASSERT_NEAR(actual[i], expected[i], kEpsilon);
  }
}

TEST(XRViewTest, ViewMatrices) {
  test::TaskEnvironment task_environment;
  const double kDepthNear = 0.1;
  const double kDepthFar = 1000.0;
  const float kFov = 52.0f;
  const int kRenderSize = 1024;

  gfx::Transform mojo_from_view;
  mojo_from_view.Translate3d(gfx::Vector3dF(4.3, 0.8, -2.5));
  mojo_from_view.RotateAboutXAxis(5.2);
  mojo_from_view.RotateAboutYAxis(30.9);
  mojo_from_view.RotateAboutZAxis(23.1);

  gfx::Transform ref_space_from_mojo;
  ref_space_from_mojo.Translate(gfx::Vector2dF(0.0, -5.0));

  gfx::Transform ref_space_from_view = ref_space_from_mojo * mojo_from_view;

  device::mojom::blink::XRViewPtr xr_view = device::mojom::blink::XRView::New();
  xr_view->eye = device::mojom::blink::XREye::kLeft;
  xr_view->field_of_view =
      device::mojom::blink::VRFieldOfView::New(kFov, kFov, kFov, kFov);
  xr_view->mojo_from_view = mojo_from_view;
  xr_view->viewport = gfx::Rect(0, 0, kRenderSize, kRenderSize);

  auto device_config = device::mojom::blink::XRSessionDeviceConfig::New();
  HashSet<device::mojom::XRSessionFeature> features = {
      device::mojom::XRSessionFeature::REF_SPACE_VIEWER};
  XRViewData* view_data = MakeGarbageCollected<XRViewData>(
      /*index=*/0, std::move(xr_view), kDepthNear, kDepthFar, *device_config,
      features, XRGraphicsBinding::Api::kWebGL);
  XRView* view =
      MakeGarbageCollected<XRView>(nullptr, view_data, ref_space_from_mojo);

  AssertMatrixEquals(GetMatrixDataForTest(view_data->MojoFromView()),
                     GetMatrixDataForTest(mojo_from_view));
  AssertMatrixEquals(
      GetMatrixDataForTest(view->refSpaceFromView()->TransformMatrix()),
      GetMatrixDataForTest(ref_space_from_view));
  AssertMatrixEquals(GetMatrixDataForTest(view_data->ProjectionMatrix()),
                     GetMatrixDataForTest(gfx::Transform::ColMajor(
                         0.78128596636, 0, 0, 0, 0, 0.78128596636, 0, 0, 0, 0,
                         -1.00020002, -1, 0, 0, -0.200020002, 0)));
}

}  // namespace
}  // namespace blink
