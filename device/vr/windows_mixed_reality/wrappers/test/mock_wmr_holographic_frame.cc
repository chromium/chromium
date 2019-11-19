// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_holographic_frame.h"

#include "device/vr/test/test_hook.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_rendering.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_timestamp.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_rendering.h"

namespace device {

// MockWMRHolographicFramePrediction
MockWMRHolographicFramePrediction::MockWMRHolographicFramePrediction() {}

MockWMRHolographicFramePrediction::~MockWMRHolographicFramePrediction() =
    default;

std::unique_ptr<WMRTimestamp> MockWMRHolographicFramePrediction::Timestamp() {
  return std::make_unique<MockWMRTimestamp>();
}

std::vector<std::unique_ptr<WMRCameraPose>>
MockWMRHolographicFramePrediction::CameraPoses() {
  std::vector<std::unique_ptr<WMRCameraPose>> ret;
  // Production code only expects a single camera pose.
  ret.push_back(std::make_unique<MockWMRCameraPose>());
  return ret;
}

// MockWMRHolographicFrame
MockWMRHolographicFrame::MockWMRHolographicFrame(
    const Microsoft::WRL::ComPtr<ID3D11Device>& device)
    : d3d11_device_(device) {}

MockWMRHolographicFrame::~MockWMRHolographicFrame() = default;

std::unique_ptr<WMRHolographicFramePrediction>
MockWMRHolographicFrame::CurrentPrediction() {
  return std::make_unique<MockWMRHolographicFramePrediction>();
}

std::unique_ptr<WMRRenderingParameters>
MockWMRHolographicFrame::TryGetRenderingParameters(const WMRCameraPose* pose) {
  // Cache a reference to the texture so that we can use it later when
  // submitting the frame.
  auto params = std::make_unique<MockWMRRenderingParameters>(d3d11_device_);
  backbuffer_texture_ = params->TryGetBackbufferAsTexture2D();
  return params;
}

bool MockWMRHolographicFrame::TryPresentUsingCurrentPrediction() {
  // A dummy frame is created when a session first starts to retrieve
  // information about the headset. Submitting this frame should not notify
  // the test hook.
  static bool first_frame = true;
  if (first_frame) {
    first_frame = false;
    return true;
  }

  // If we don't actually have a texture to submit, don't try.
  if (!backbuffer_texture_)
    return false;
  // Set eye-independent data, copy, then set eye-dependent data.
  SubmittedFrameData left_data;
  auto viewport = CurrentPrediction()->CameraPoses().front()->Viewport();
  left_data.viewport = {viewport.X, viewport.Y, viewport.X + viewport.Width,
                        viewport.Y + viewport.Height};

  D3D11_TEXTURE2D_DESC desc;
  backbuffer_texture_->GetDesc(&desc);
  left_data.image_width = desc.Width;
  left_data.image_height = desc.Height;

  SubmittedFrameData right_data = left_data;
  left_data.left_eye = true;
  right_data.left_eye = false;

  bool success =
      CopyTextureDataIntoFrameData(&left_data, 0 /*index, left eye */);
  DCHECK(success);
  success = CopyTextureDataIntoFrameData(&right_data, 1 /* index, right eye */);
  DCHECK(success);

  auto locked_hook = MixedRealityDeviceStatics::GetLockedTestHook();
  if (locked_hook.GetHook()) {
    locked_hook.GetHook()->OnFrameSubmitted(left_data);
    locked_hook.GetHook()->OnFrameSubmitted(right_data);
  }
  return true;
}

bool MockWMRHolographicFrame::CopyTextureDataIntoFrameData(
    SubmittedFrameData* data,
    unsigned int index) {
  DCHECK(d3d11_device_);
  DCHECK(backbuffer_texture_);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(&context);

  size_t buffer_size = sizeof(device::SubmittedFrameData::raw_buffer);
  size_t buffer_size_pixels = buffer_size / sizeof(device::Color);

  // We copy the submitted texture to a new texture, so we can map it, and
  // read back pixel data.
  auto desc = CD3D11_TEXTURE2D_DESC();
  desc.ArraySize = 1;
  desc.Width = buffer_size_pixels;
  desc.Height = 1;
  desc.MipLevels = 1;
  desc.SampleDesc = {1, 0};
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_copy;
  HRESULT hr = d3d11_device_->CreateTexture2D(&desc, nullptr, &texture_copy);
  if (FAILED(hr))
    return false;

  // A strip of pixels along the top of the texture, however many will fit into
  // our buffer.
  D3D11_BOX box = {0, 0, 0, buffer_size_pixels, 1, 1};
  context->CopySubresourceRegion(texture_copy.Get(), 0, 0, 0, 0,
                                 backbuffer_texture_.Get(), index, &box);

  D3D11_MAPPED_SUBRESOURCE map_data = {};
  hr = context->Map(texture_copy.Get(), 0, D3D11_MAP_READ, 0, &map_data);
  if (FAILED(hr))
    return false;

  // We have a 1-pixel image, so store it in the provided SubmittedFrameData
  // along with the raw data.
  device::Color* color = reinterpret_cast<device::Color*>(map_data.pData);
  data->color = color[0];
  memcpy(&data->raw_buffer, map_data.pData, buffer_size);

  context->Unmap(texture_copy.Get(), 0);
  return true;
}

}  // namespace device
