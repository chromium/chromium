// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_rendering.h"

#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgiformat.h>

#include "base/logging.h"
#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "ui/gfx/transform.h"

namespace device {

// MockWMRCamera
MockWMRCamera::MockWMRCamera() {}

MockWMRCamera::~MockWMRCamera() = default;

ABI::Windows::Foundation::Size MockWMRCamera::RenderTargetSize() {
  ABI::Windows::Foundation::Size ret;
  ret.Width = kDefaultWmrRenderWidth;
  ret.Height = kDefaultWmrRenderHeight;
  return ret;
}

bool MockWMRCamera::IsStereo() {
  return true;
}

// MockWMRCameraPose
MockWMRCameraPose::MockWMRCameraPose() {}

MockWMRCameraPose::~MockWMRCameraPose() = default;

ABI::Windows::Foundation::Rect MockWMRCameraPose::Viewport() {
  ABI::Windows::Foundation::Rect ret;
  ret.X = 0;
  ret.Y = 0;
  ret.Width = kDefaultWmrRenderWidth;
  ret.Height = kDefaultWmrRenderHeight;
  return ret;
}

std::unique_ptr<WMRCamera> MockWMRCameraPose::HolographicCamera() {
  return std::make_unique<MockWMRCamera>();
}

void FrustumToMatrix(float left,
                     float right,
                     float top,
                     float bottom,
                     ABI::Windows::Foundation::Numerics::Matrix4x4* m) {
  float x_scale = 2.0f / (left + right);
  float y_scale = 2.0f / (top + bottom);
  // We don't actually care about the near/far planes, so set to arbitrary
  // sane values.
  float n = 0.1f;
  float f = 100.0f;
  float inv_nf = 1.0f / (n - f);

  m->M11 = x_scale;
  m->M21 = 0.0f;
  m->M31 = 0.0f;
  m->M41 = 0.0f;
  m->M12 = 0.0f;
  m->M22 = y_scale;
  m->M32 = 0.0f;
  m->M42 = 0.0f;
  m->M13 = -((left - right) * x_scale * 0.5);
  m->M23 = ((top - bottom) * y_scale * 0.5);
  m->M33 = (n + f) * inv_nf;
  m->M43 = -1.0f;
  m->M14 = 0.0f;
  m->M24 = 0.0f;
  // TODO(https://crbug.com/931376): Make this go between 0 and 1 instead of
  // -1 and 1 when adding tests for decomposition and recomposition of
  // projection matrices.
  m->M34 = (2.0f * f * n) * inv_nf;
  m->M44 = 0.0f;
}

ABI::Windows::Graphics::Holographic::HolographicStereoTransform
MockWMRCameraPose::ProjectionTransform() {
  ABI::Windows::Graphics::Holographic::HolographicStereoTransform ret;
  auto hook = MixedRealityDeviceStatics::GetLockedTestHook();

  if (!hook.GetHook())
    return ret;

  auto device_config = hook.GetHook()->WaitGetDeviceConfig();
  auto* frustum_left = device_config.viewport_left;
  auto* frustum_right = device_config.viewport_right;

  // TODO: Properly reuse some code instead of copying from
  // XRView::UpdateProjectionMatrixFromFoV.
  FrustumToMatrix(frustum_left[0], frustum_left[1], frustum_left[2],
                  frustum_left[3], &ret.Left);
  FrustumToMatrix(frustum_right[0], frustum_right[1], frustum_right[2],
                  frustum_right[3], &ret.Right);
  return ret;
}

void CopyRowMajorFloatArrayToWindowsMatrix(
    float t[16],
    ABI::Windows::Foundation::Numerics::Matrix4x4& matrix) {
  matrix = {t[0], t[1], t[2],  t[3],  t[4],  t[5],  t[6],  t[7],
            t[8], t[9], t[10], t[11], t[12], t[13], t[14], t[15]};
}

bool MockWMRCameraPose::TryGetViewTransform(
    const WMRCoordinateSystem* origin,
    ABI::Windows::Graphics::Holographic::HolographicStereoTransform*
        transform) {
  // No idea if the view transform should be the same for both.
  auto hook = MixedRealityDeviceStatics::GetLockedTestHook();

  if (!hook.GetHook())
    return false;

  auto pose_data = hook.GetHook()->WaitGetPresentingPose();
  if (!pose_data.is_valid)
    return false;

  // We need to get the inverse of the given transform, as it's the
  // device-to-origin transform and we need the origin-to-device transform.
  gfx::Transform device_to_origin = PoseFrameDataToTransform(pose_data);
  gfx::Transform origin_to_device = device_to_origin;
  auto success = origin_to_device.GetInverse(&origin_to_device);
  DCHECK(success);
  float col_major_transform[16];
  origin_to_device.matrix().asColMajorf(col_major_transform);

  // index of matrix[3][0] in 1d array
  int index = 3 * 4;
  float original_x = col_major_transform[index];
  float ipd = hook.GetHook()->WaitGetDeviceConfig().interpupillary_distance;
  col_major_transform[index] = original_x - ipd / 2;
  CopyRowMajorFloatArrayToWindowsMatrix(col_major_transform, transform->Left);
  col_major_transform[index] = original_x + ipd / 2;
  CopyRowMajorFloatArrayToWindowsMatrix(col_major_transform, transform->Right);

  return true;
}

MockWMRRenderingParameters::MockWMRRenderingParameters(
    const Microsoft::WRL::ComPtr<ID3D11Device>& device)
    : d3d11_device_(device) {}

MockWMRRenderingParameters::~MockWMRRenderingParameters() = default;

Microsoft::WRL::ComPtr<ID3D11Texture2D>
MockWMRRenderingParameters::TryGetBackbufferAsTexture2D() {
  if (backbuffer_texture_)
    return backbuffer_texture_;
  if (!d3d11_device_)
    return nullptr;
  auto desc = CD3D11_TEXTURE2D_DESC();
  desc.ArraySize = 2;
  desc.Width = kDefaultWmrRenderWidth;
  desc.Height = kDefaultWmrRenderHeight;
  desc.MipLevels = 1;
  desc.SampleDesc = {1, 0};
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;

  auto hr =
      d3d11_device_->CreateTexture2D(&desc, nullptr, &backbuffer_texture_);
  if (FAILED(hr))
    return nullptr;

  return backbuffer_texture_;
}

}  // namespace device
