// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_device.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/transform_util.h"

namespace device {

FakeVRDevice::FakeVRDevice(mojom::XRDeviceId id) : VRDeviceBase(id) {
  SetVRDisplayInfo(InitBasicDevice());
}

FakeVRDevice::~FakeVRDevice() {}

mojom::VRDisplayInfoPtr FakeVRDevice::InitBasicDevice() {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->id = GetId();

  display_info->left_eye = InitEye(45, -0.03f, 1024);
  display_info->right_eye = InitEye(45, 0.03f, 1024);
  return display_info;
}

mojom::VREyeParametersPtr FakeVRDevice::InitEye(float fov,
                                                float offset,
                                                uint32_t size) {
  mojom::VREyeParametersPtr eye = mojom::VREyeParameters::New();

  eye->field_of_view = mojom::VRFieldOfView::New();
  eye->field_of_view->up_degrees = fov;
  eye->field_of_view->down_degrees = fov;
  eye->field_of_view->left_degrees = fov;
  eye->field_of_view->right_degrees = fov;

  gfx::DecomposedTransform decomp;
  decomp.translate[0] = offset;
  eye->head_from_eye = gfx::ComposeTransform(decomp);

  eye->render_width = size;
  eye->render_height = size;

  return eye;
}

void FakeVRDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  OnStartPresenting();
  // The current tests never use the return values, so it's fine to return
  // invalid data here.
  std::move(callback).Run(nullptr, mojo::NullRemote());
}

void FakeVRDevice::OnPresentingControllerMojoConnectionError() {
  OnExitPresent();
}

}  // namespace device
