// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/geometry/transform_util.h"

namespace device {

FakeVRDevice::FakeVRDevice(mojom::XRDeviceId id) : VRDeviceBase(id) {
  SetVRDisplayInfo(InitBasicDevice());
}

FakeVRDevice::~FakeVRDevice() {}

mojom::VRDisplayInfoPtr FakeVRDevice::InitBasicDevice() {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->views.resize(2);
  display_info->views[0] = InitView(mojom::XREye::kLeft, 45, -0.03f, 1024);
  display_info->views[1] = InitView(mojom::XREye::kRight, 45, 0.03f, 1024);

  return display_info;
}

mojom::XRViewPtr FakeVRDevice::InitView(mojom::XREye eye,
                                        float fov,
                                        float offset,
                                        uint32_t size) {
  mojom::XRViewPtr view = mojom::XRView::New();
  view->eye = eye;

  view->field_of_view = mojom::VRFieldOfView::New();
  view->field_of_view->up_degrees = fov;
  view->field_of_view->down_degrees = fov;
  view->field_of_view->left_degrees = fov;
  view->field_of_view->right_degrees = fov;

  gfx::DecomposedTransform decomp;
  decomp.translate[0] = offset;
  view->mojo_from_view = gfx::ComposeTransform(decomp);

  view->viewport = gfx::Rect(0, 0, size, size);

  return view;
}

void FakeVRDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  OnStartPresenting();
  // The current tests never use the return values, so it's fine to return
  // invalid data here.
  std::move(callback).Run(nullptr);
}

void FakeVRDevice::OnPresentingControllerMojoConnectionError() {
  OnExitPresent();
}

}  // namespace device
