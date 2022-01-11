// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/renderer/web_engine_audio_device_factory.h"

#include <fuchsia/media/cpp/fidl.h>

#include "base/check.h"
#include "content/public/renderer/render_frame.h"
#include "media/base/audio_renderer_sink.h"
#include "media/fuchsia/audio/fuchsia_audio_capturer_source.h"
#include "media/fuchsia/audio/fuchsia_audio_output_device.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider.mojom.h"
#include "media/fuchsia/mojom/fuchsia_media_resource_provider_mojom_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_frame.h"

namespace {

content::RenderFrame* GetRenderFrameForToken(
    const blink::LocalFrameToken& frame_token) {
  auto* web_frame = blink::WebFrame::FromFrameToken(frame_token);
  if (!web_frame)
    return nullptr;

  int render_frame_id =
      content::RenderFrame::GetRoutingIdForWebFrame(web_frame);
  return content::RenderFrame::FromRoutingID(render_frame_id);
}

}  // namespace

WebEngineAudioDeviceFactory::WebEngineAudioDeviceFactory()
    : audio_capturer_thread_("AudioCapturerThread") {}
WebEngineAudioDeviceFactory::~WebEngineAudioDeviceFactory() = default;

scoped_refptr<media::AudioRendererSink>
WebEngineAudioDeviceFactory::CreateFinalAudioRendererSink(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  // Return nullptr to fallback to the default renderer implementation.
  return nullptr;
}

scoped_refptr<media::AudioRendererSink>
WebEngineAudioDeviceFactory::CreateAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  switch (source_type) {
    case blink::WebAudioDeviceSourceType::kMediaElement:
      // MediaElement uses CreateSwitchableAudioRendererSink().
      CHECK(false);
      return nullptr;

    case blink::WebAudioDeviceSourceType::kWebRtc:
    case blink::WebAudioDeviceSourceType::kNonRtcAudioTrack:
      // Return nullptr for WebRTC streams. This will cause the caller to
      // fallback to AudioOutputDevice, which outputs through
      // AudioOutputStreamFuchsia.
      return nullptr;

    // kNone is used in AudioDeviceFactory::GetOutputDeviceInfo() to get
    // default output device params.
    case blink::WebAudioDeviceSourceType::kNone:
      break;

    // Create WebEngineAudioDeviceFactory for all WebAudio.
    case blink::WebAudioDeviceSourceType::kWebAudioInteractive:
    case blink::WebAudioDeviceSourceType::kWebAudioBalanced:
    case blink::WebAudioDeviceSourceType::kWebAudioPlayback:
    case blink::WebAudioDeviceSourceType::kWebAudioExact:
      break;
  }

  auto* render_frame = GetRenderFrameForToken(frame_token);
  CHECK(render_frame);

  // Connect FuchsiaMediaResourceProvider.
  mojo::Remote<media::mojom::FuchsiaMediaResourceProvider>
      media_resource_provider;
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      media_resource_provider.BindNewPipeAndPassReceiver());

  // If AudioConsumer is not enabled then fallback to AudioOutputDevice.
  bool use_audio_consumer = false;
  if (!media_resource_provider->ShouldUseAudioConsumer(&use_audio_consumer) ||
      !use_audio_consumer) {
    return nullptr;
  }

  // AudioConsumer can be used only to output to the default device.
  if (!params.device_id.empty())
    return nullptr;

  // Connect AudioConsumer.
  fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer;
  media_resource_provider->CreateAudioConsumer(audio_consumer.NewRequest());

  return media::FuchsiaAudioOutputDevice::CreateOnDefaultThread(
      std::move(audio_consumer));
}

scoped_refptr<media::SwitchableAudioRendererSink>
WebEngineAudioDeviceFactory::CreateSwitchableAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  // Return nullptr to fallback to the default renderer implementation.
  return nullptr;
}

scoped_refptr<media::AudioCapturerSource>
WebEngineAudioDeviceFactory::CreateAudioCapturerSource(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSourceParameters& params) {
  auto* render_frame = GetRenderFrameForToken(frame_token);
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

  if (!audio_capturer_thread_.IsRunning()) {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    options.priority = base::ThreadPriority::REALTIME_AUDIO;
    audio_capturer_thread_.StartWithOptions(std::move(options));
  }

  return base::MakeRefCounted<media::FuchsiaAudioCapturerSource>(
      std::move(capturer), audio_capturer_thread_.task_runner());
}
