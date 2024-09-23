// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/renderer/web_engine_audio_device_factory.h"

#include <fuchsia/media/cpp/fidl.h>

#include "base/check.h"
#include "content/public/renderer/render_frame.h"
#include "fuchsia_web/webengine/mojom/web_engine_media_resource_provider.mojom.h"
#include "fuchsia_web/webengine/renderer/web_engine_audio_output_device.h"
#include "media/base/audio_renderer_sink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {

content::RenderFrame* GetRenderFrameForToken(
    const blink::LocalFrameToken& frame_token) {
  auto* web_frame = blink::WebLocalFrame::FromFrameToken(frame_token);
  if (!web_frame)
    return nullptr;

  return content::RenderFrame::FromWebFrame(web_frame);
}

}  // namespace

WebEngineAudioDeviceFactory::WebEngineAudioDeviceFactory() = default;
WebEngineAudioDeviceFactory::~WebEngineAudioDeviceFactory() = default;

scoped_refptr<media::AudioRendererSink>
WebEngineAudioDeviceFactory::NewAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  bool allow_audio_consumer = true;
  switch (source_type) {
    case blink::WebAudioDeviceSourceType::kMediaElement:
      // MediaElement uses NewMixableSink().
      NOTREACHED();

    case blink::WebAudioDeviceSourceType::kWebRtc:
    case blink::WebAudioDeviceSourceType::kNonRtcAudioTrack:
      // AudioConsumer is not enabled for WebRTC streams yet.
      allow_audio_consumer = false;
      break;

    // kNone is used in AudioDeviceFactory::GetOutputDeviceInfo() to get
    // default output device params.
    case blink::WebAudioDeviceSourceType::kNone:
      break;

    // Create WebEngineAudioDeviceFactory for all WebAudio streams.
    case blink::WebAudioDeviceSourceType::kWebAudioInteractive:
    case blink::WebAudioDeviceSourceType::kWebAudioBalanced:
    case blink::WebAudioDeviceSourceType::kWebAudioPlayback:
    case blink::WebAudioDeviceSourceType::kWebAudioExact:
      break;
  }

  // AudioConsumer can be used only to output to the default device.
  if (!params.device_id.empty())
    allow_audio_consumer = false;

  mojo::Remote<mojom::WebEngineMediaResourceProvider> media_resource_provider;
  bool use_audio_consumer = false;
  if (allow_audio_consumer) {
    auto* render_frame = GetRenderFrameForToken(frame_token);
    CHECK(render_frame);

    // Connect WebEngineMediaResourceProvider.
    render_frame->GetBrowserInterfaceBroker().GetInterface(
        media_resource_provider.BindNewPipeAndPassReceiver());

    bool result =
        media_resource_provider->ShouldUseAudioConsumer(&use_audio_consumer);
    DCHECK(result);
  }

  // If AudioConsumer is not enabled then fallback to AudioOutputDevice.
  if (!use_audio_consumer) {
    return AudioDeviceFactory::NewAudioRendererSink(source_type, frame_token,
                                                    params);
  }

  // Connect AudioConsumer.
  fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer;
  media_resource_provider->CreateAudioConsumer(audio_consumer.NewRequest());

  return WebEngineAudioOutputDevice::CreateOnDefaultThread(
      std::move(audio_consumer));
}
