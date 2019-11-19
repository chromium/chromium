// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/audio_device_factory.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/common/content_constants_internal.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "content/renderer/media/audio/audio_input_ipc_factory.h"
#include "content/renderer/media/audio/audio_output_ipc_factory.h"
#include "content/renderer/media/audio/audio_renderer_mixer_manager.h"
#include "content/renderer/media/audio/audio_renderer_sink_cache_impl.h"
#include "content/renderer/media/audio/mojo_audio_input_ipc.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_input_device.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/media_switches.h"

namespace content {

// static
AudioDeviceFactory* AudioDeviceFactory::factory_ = nullptr;

namespace {

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
// Due to driver deadlock issues on Windows (http://crbug/422522) there is a
// chance device authorization response is never received from the browser side.
// In this case we will time out, to avoid renderer hang forever waiting for
// device authorization (http://crbug/615589). This will result in "no audio".
// There are also cases when authorization takes too long on Mac and Linux.
constexpr int64_t kMaxAuthorizationTimeoutMs = 10000;
#else
constexpr int64_t kMaxAuthorizationTimeoutMs = 0;  // No timeout.
#endif

base::TimeDelta GetDefaultAuthTimeout() {
  // Set authorization request timeout at 80% of renderer hung timeout,
  // but no more than kMaxAuthorizationTimeout.
  return base::TimeDelta::FromMilliseconds(
      std::min(kHungRendererDelayMs * 8 / 10, kMaxAuthorizationTimeoutMs));
}

scoped_refptr<media::AudioOutputDevice> NewOutputDevice(
    int render_frame_id,
    const media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  auto device = base::MakeRefCounted<media::AudioOutputDevice>(
      AudioOutputIPCFactory::get()->CreateAudioOutputIPC(render_frame_id),
      AudioOutputIPCFactory::get()->io_task_runner(), params, auth_timeout);
  device->RequestDeviceAuthorization();
  return device;
}

// This is where we decide which audio will go to mixers and which one to
// AudioOutputDevice directly.
bool IsMixable(blink::WebAudioDeviceSourceType source_type) {
  // Media element must ALWAYS go through mixer.
  return source_type == blink::WebAudioDeviceSourceType::kMediaElement;
}

scoped_refptr<media::SwitchableAudioRendererSink> NewMixableSink(
    blink::WebAudioDeviceSourceType source_type,
    int render_frame_id,
    const media::AudioSinkParameters& params) {
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  DCHECK(render_thread) << "RenderThreadImpl is not instantiated, or "
                        << "GetOutputDeviceInfo() is called on a wrong thread ";
  DCHECK(!params.processing_id.has_value());
  return render_thread->GetAudioRendererMixerManager()->CreateInput(
      render_frame_id, params.session_id, params.device_id,
      AudioDeviceFactory::GetSourceLatencyType(source_type));
}

}  // namespace

media::AudioLatency::LatencyType AudioDeviceFactory::GetSourceLatencyType(
    blink::WebAudioDeviceSourceType source) {
  switch (source) {
    case blink::WebAudioDeviceSourceType::kWebAudioInteractive:
      return media::AudioLatency::LATENCY_INTERACTIVE;
    case blink::WebAudioDeviceSourceType::kNone:
    case blink::WebAudioDeviceSourceType::kWebRtc:
    case blink::WebAudioDeviceSourceType::kNonRtcAudioTrack:
    case blink::WebAudioDeviceSourceType::kWebAudioBalanced:
      return media::AudioLatency::LATENCY_RTC;
    case blink::WebAudioDeviceSourceType::kMediaElement:
    case blink::WebAudioDeviceSourceType::kWebAudioPlayback:
      return media::AudioLatency::LATENCY_PLAYBACK;
    case blink::WebAudioDeviceSourceType::kWebAudioExact:
      return media::AudioLatency::LATENCY_EXACT_MS;
  }
  NOTREACHED();
  return media::AudioLatency::LATENCY_INTERACTIVE;
}

scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererMixerSink(
    int render_frame_id,
    const media::AudioSinkParameters& params) {
  // AudioRendererMixer sinks are always used asynchronously and thus can
  // operate without a timeout value.
  return NewFinalAudioRendererSink(render_frame_id, params, base::TimeDelta());
}

// static
scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    int render_frame_id,
    const media::AudioSinkParameters& params) {
  if (factory_) {
    scoped_refptr<media::AudioRendererSink> device =
        factory_->CreateAudioRendererSink(source_type, render_frame_id, params);
    if (device)
      return device;
  }

  // Perhaps streams with a processing ID just shouldn't be mixable, i.e. call
  // NewFinalAudioRendererSink for them rather than DCHECK?
  DCHECK(!(params.processing_id.has_value() && IsMixable(source_type)));

  if (IsMixable(source_type))
    return NewMixableSink(source_type, render_frame_id, params);

  UMA_HISTOGRAM_BOOLEAN("Media.Audio.Render.SinkCache.UsedForSinkCreation",
                        false);
  return NewFinalAudioRendererSink(render_frame_id, params,
                                   GetDefaultAuthTimeout());
}

// static
scoped_refptr<media::SwitchableAudioRendererSink>
AudioDeviceFactory::NewSwitchableAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    int render_frame_id,
    const media::AudioSinkParameters& params) {
  if (factory_) {
    scoped_refptr<media::SwitchableAudioRendererSink> sink =
        factory_->CreateSwitchableAudioRendererSink(source_type,
                                                    render_frame_id, params);
    if (sink)
      return sink;
  }

  if (IsMixable(source_type))
    return NewMixableSink(source_type, render_frame_id, params);

  // AudioOutputDevice is not RestartableAudioRendererSink, so we can't return
  // anything for those who wants to create an unmixable sink.
  NOTIMPLEMENTED();
  return nullptr;
}

// static
scoped_refptr<media::AudioCapturerSource>
AudioDeviceFactory::NewAudioCapturerSource(
    int render_frame_id,
    const media::AudioSourceParameters& params) {
  if (factory_) {
    // We don't pass on |session_id|, as this branch is only used for tests.
    scoped_refptr<media::AudioCapturerSource> source =
        factory_->CreateAudioCapturerSource(render_frame_id, params);
    if (source)
      return source;
  }

  return base::MakeRefCounted<media::AudioInputDevice>(
      AudioInputIPCFactory::get()->CreateAudioInputIPC(render_frame_id, params),
      media::AudioInputDevice::Purpose::kUserInput);
}

// static
media::OutputDeviceInfo AudioDeviceFactory::GetOutputDeviceInfo(
    int render_frame_id,
    const media::AudioSinkParameters& params) {
  DCHECK(RenderThreadImpl::current())
      << "RenderThreadImpl is not instantiated, or "
      << "GetOutputDeviceInfo() is called on a wrong thread ";

  constexpr base::TimeDelta kDeleteTimeout =
      base::TimeDelta::FromMilliseconds(5000);

  // There's one process wide instance that lives on the render thread.
  static base::NoDestructor<AudioRendererSinkCacheImpl> cache(
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      base::BindRepeating(&AudioDeviceFactory::NewAudioRendererSink,
                          blink::WebAudioDeviceSourceType::kNone),
      kDeleteTimeout);
  return cache->GetSinkInfo(render_frame_id, params.session_id,
                            params.device_id);
}

AudioDeviceFactory::AudioDeviceFactory() {
  DCHECK(!factory_) << "Can't register two factories at once.";
  factory_ = this;
}

AudioDeviceFactory::~AudioDeviceFactory() {
  factory_ = nullptr;
}

// static
scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewFinalAudioRendererSink(
    int render_frame_id,
    const media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  if (factory_) {
    scoped_refptr<media::AudioRendererSink> sink =
        factory_->CreateFinalAudioRendererSink(render_frame_id, params,
                                               auth_timeout);
    if (sink)
      return sink;
  }

  return NewOutputDevice(render_frame_id, params, auth_timeout);
}

}  // namespace content
