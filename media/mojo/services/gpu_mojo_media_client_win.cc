// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/win/windows_version.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_switches.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"
#include "media/gpu/windows/d3d11_video_decoder.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_angle_util_win.h"

namespace media {

namespace {

D3D11VideoDecoder::GetD3D11DeviceCB GetD3D11DeviceCallback() {
  return base::BindRepeating(
      []() { return gl::QueryD3D11DeviceObjectFromANGLE(); });
}

bool ShouldUseD3D11VideoDecoder(
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds) {
  if (!base::FeatureList::IsEnabled(kD3D11VideoDecoder))
    return false;
  if (gpu_workarounds.disable_d3d11_video_decoder)
    return false;
  if (base::win::GetVersion() == base::win::Version::WIN7)
    return false;
  return true;
}

class WinPlatformDelegate : public GpuMojoMediaClient::PlatformDelegate {
 public:
  explicit WinPlatformDelegate(GpuMojoMediaClient* client) : client_(client) {}
  ~WinPlatformDelegate() override = default;

  WinPlatformDelegate(const WinPlatformDelegate&) = delete;
  void operator=(const WinPlatformDelegate&) = delete;

  // GpuMojoMediaClient::PlatformDelegate implementation.
  SupportedVideoDecoderConfigs GetSupportedVideoDecoderConfigsSync() {
    SupportedVideoDecoderConfigs supported_configs;
    if (ShouldUseD3D11VideoDecoder(client_->gpu_workarounds())) {
      return D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
          client_->gpu_preferences(), client_->gpu_workarounds(),
          GetD3D11DeviceCallback());
    } else if (!client_->gpu_workarounds().disable_dxva_video_decoder) {
      return client_->GetVDAVideoDecoderConfigs();
    } else {
      return {};
    }
  }

  // GpuMojoMediaClient::PlatformDelegate implementation.
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const VideoDecoderTraits& traits) override {
    if (!ShouldUseD3D11VideoDecoder(client_->gpu_workarounds())) {
      if (client_->gpu_workarounds().disable_dxva_video_decoder)
        return nullptr;
      return VdaVideoDecoder::Create(
          traits.task_runner, traits.gpu_task_runner, traits.media_log->Clone(),
          *traits.target_color_space, client_->gpu_preferences(),
          client_->gpu_workarounds(), traits.get_command_buffer_stub_cb);
    }
    DCHECK(base::FeatureList::IsEnabled(kD3D11VideoDecoder));
    return D3D11VideoDecoder::Create(
        traits.gpu_task_runner, traits.media_log->Clone(),
        client_->gpu_preferences(), client_->gpu_workarounds(),
        traits.get_command_buffer_stub_cb, GetD3D11DeviceCallback(),
        GetSupportedVideoDecoderConfigsSync(),
        gl::DirectCompositionSurfaceWin::IsHDRSupported());
  }

  void GetSupportedVideoDecoderConfigs(
      MojoMediaClient::SupportedVideoDecoderConfigsCallback callback) override {
    std::move(callback).Run(GetSupportedVideoDecoderConfigsSync());
  }

  VideoDecoderType GetDecoderImplementationType() override {
    if (!ShouldUseD3D11VideoDecoder(client_->gpu_workarounds()))
      return VideoDecoderType::kVda;
    return VideoDecoderType::kD3D11;
  }

 private:
  GpuMojoMediaClient* client_;
};

}  // namespace

std::unique_ptr<GpuMojoMediaClient::PlatformDelegate>
GpuMojoMediaClient::PlatformDelegate::Create(GpuMojoMediaClient* client) {
  return std::make_unique<WinPlatformDelegate>(client);
}

}  // namespace media
