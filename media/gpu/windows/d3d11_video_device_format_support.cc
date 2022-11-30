// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_device_format_support.h"

namespace media {

FormatSupportChecker::FormatSupportChecker(ComD3D11Device device)
    : device_(std::move(device)) {
}

FormatSupportChecker::~FormatSupportChecker() {
  if (initialized_)
    enumerator_.Reset();
  device_.Reset();
}

bool FormatSupportChecker::Initialize() {
  ComD3D11VideoDevice v_device;
  if (!device_)
    return false;

  if (!SUCCEEDED(device_.As(&v_device)))
    return false;

  // The values here should have _no_ effect on supported profiles, but they
  // are needed anyway for initialization.
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc;
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = 1920;
  desc.InputHeight = 1080;
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = 1920;
  desc.OutputHeight = 1080;
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  if (!SUCCEEDED(v_device->CreateVideoProcessorEnumerator(&desc, &enumerator_)))
    return false;

  // For tests, which don't provide one (successfully).
  if (!enumerator_)
    return false;

  // Check that the |CheckFormatSupport| and |CheckVideoProcessorFormat| calls
  // won't be failing
  UINT unneeded = 0;
  DXGI_FORMAT example = DXGI_FORMAT_NV12;
  if (!SUCCEEDED(device_->CheckFormatSupport(example, &unneeded)))
    return false;

  if (!SUCCEEDED(enumerator_->CheckVideoProcessorFormat(example, &unneeded)))
    return false;

  initialized_ = true;
  return true;
}

bool FormatSupportChecker::CheckOutputFormatSupport(DXGI_FORMAT format) const {
  if (!device_ || !enumerator_)
    return false;

  DCHECK(initialized_);

  UINT device = 0, enumerator = 0;
  if (!SUCCEEDED(device_->CheckFormatSupport(format, &device)))
    return false;
  if (!SUCCEEDED(enumerator_->CheckVideoProcessorFormat(format, &enumerator)))
    return false;

  return (enumerator & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) &&
         (device & D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT);
}

}  // namespace media
