// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/fuchsia_audio_device_factory.h"

#include <fuchsia/media/cpp/fidl.h>

#include "content/public/renderer/render_frame.h"
#include "media/base/audio_renderer_sink.h"
#include "media/fuchsia/audio/fuchsia_audio_capturer_source.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider.mojom.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider_mojom_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_frame.h"

namespace content {

FuchsiaAudioDeviceFactory::FuchsiaAudioDeviceFactory() = default;
FuchsiaAudioDeviceFactory::~FuchsiaAudioDeviceFactory() = default;

scoped_refptr<media::AudioRendererSink>
FuchsiaAudioDeviceFactory::CreateFinalAudioRendererSink(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  // Return nullptr to fallback to the default renderer implementation.
  return nullptr;
}

scoped_refptr<media::AudioRendererSink>
FuchsiaAudioDeviceFactory::CreateAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  // Return nullptr to fallback to the default renderer implementation.
  return nullptr;
}

scoped_refptr<media::SwitchableAudioRendererSink>
FuchsiaAudioDeviceFactory::CreateSwitchableAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  // Return nullptr to fallback to the default renderer implementation.
  return nullptr;
}

scoped_refptr<media::AudioCapturerSource>
FuchsiaAudioDeviceFactory::CreateAudioCapturerSource(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSourceParameters& params) {
  blink::WebFrame* web_frame = blink::WebFrame::FromFrameToken(frame_token);
  if (!web_frame)
    return nullptr;

  int render_frame_id = RenderFrame::GetRoutingIdForWebFrame(web_frame);
  auto* render_frame = RenderFrame::FromRoutingID(render_frame_id);
  if (!render_frame)
    return nullptr;

  // Connect FuchsiaMediaResourceProvider.
  mojo::Remote<media::mojom::FuchsiaMediaResourceProvider>
      media_resource_provider;
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      media_resource_provider.BindNewPipeAndPassReceiver());

  // Connect AudioCapturer.
  fidl::InterfaceHandle<fuchsia::media::AudioCapturer> capturer;
  media_resource_provider->CreateAudioCapturer(capturer.NewRequest());

  return base::MakeRefCounted<media::FuchsiaAudioCapturerSource>(
      std::move(capturer));
}

}  // namespace content