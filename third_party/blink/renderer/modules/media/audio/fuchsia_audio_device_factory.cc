// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/media/audio/fuchsia_audio_device_factory.h"

#include <fuchsia/media/cpp/fidl.h>

#include "media/base/audio_renderer_sink.h"
#include "media/fuchsia/audio/fuchsia_audio_capturer_source.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider.mojom-blink.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider_mojom_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

FuchsiaAudioDeviceFactory::FuchsiaAudioDeviceFactory() = default;
FuchsiaAudioDeviceFactory::~FuchsiaAudioDeviceFactory() = default;

scoped_refptr<media::AudioRendererSink>
FuchsiaAudioDeviceFactory::CreateFinalAudioRendererSink(
    const LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  // Return nullptr to fallback to the default renderer implementation.
  return nullptr;
}

scoped_refptr<media::AudioRendererSink>
FuchsiaAudioDeviceFactory::CreateAudioRendererSink(
    WebAudioDeviceSourceType source_type,
    const LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  // Return nullptr to fallback to the default renderer implementation.
  return nullptr;
}

scoped_refptr<media::SwitchableAudioRendererSink>
FuchsiaAudioDeviceFactory::CreateSwitchableAudioRendererSink(
    WebAudioDeviceSourceType source_type,
    const LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  // Return nullptr to fallback to the default renderer implementation.
  return nullptr;
}

scoped_refptr<media::AudioCapturerSource>
FuchsiaAudioDeviceFactory::CreateAudioCapturerSource(
    const LocalFrameToken& frame_token,
    const media::AudioSourceParameters& params) {
  auto* local_frame = LocalFrame::FromFrameToken(frame_token);
  if (!local_frame)
    return nullptr;

  // Connect FuchsiaMediaResourceProvider.
  mojo::Remote<media::mojom::blink::FuchsiaMediaResourceProvider>
      media_resource_provider;
  local_frame->GetBrowserInterfaceBroker().GetInterface(
      media_resource_provider.BindNewPipeAndPassReceiver());

  // Connect AudioCapturer.
  fidl::InterfaceHandle<fuchsia::media::AudioCapturer> capturer;
  media_resource_provider->CreateAudioCapturer(capturer.NewRequest());

  return base::MakeRefCounted<media::FuchsiaAudioCapturerSource>(
      std::move(capturer));
}

}  // namespace blink
