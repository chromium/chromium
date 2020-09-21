// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"

#include <algorithm>

#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "media/audio/audio_input_device.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_input_ipc_factory.h"
#include "third_party/blink/public/web/modules/media/audio/web_audio_output_ipc_factory.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_manager.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_sink_cache_impl.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// static
AudioDeviceFactory* AudioDeviceFactory::factory_ = nullptr;

namespace {

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
// Due to driver deadlock issues on Windows (http://crbug/422522) there is a
// chance device authorization response is never received from the browser side.
// In this case we will time out, to avoid renderer hang forever waiting for
// device authorization (http://crbug/615589). This will result in "no audio".
// There are also cases when authorization takes too long on Mac and Linux.
constexpr base::TimeDelta kMaxAuthorizationTimeout =
    base::TimeDelta::FromSeconds(10);
#else
constexpr base::TimeDelta kMaxAuthorizationTimeout;  // No timeout.
#endif

base::TimeDelta GetDefaultAuthTimeout() {
  // Set authorization request timeout at 80% of renderer hung timeout,
  // but no more than kMaxAuthorizationTimeout.
  return std::min(Platform::Current()->GetHungRendererDelay() * 8 / 10,
                  kMaxAuthorizationTimeout);
}

scoped_refptr<media::AudioOutputDevice> NewOutputDevice(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  CHECK(blink::WebAudioOutputIPCFactory::GetInstance().io_task_runner());
  auto device = base::MakeRefCounted<media::AudioOutputDevice>(
      blink::WebAudioOutputIPCFactory::GetInstance().CreateAudioOutputIPC(
          frame_token),
      blink::WebAudioOutputIPCFactory::GetInstance().io_task_runner(), params,
      auth_timeout);
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
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  DCHECK(IsMainThread()) << __func__ << "() is called on a wrong thread.";
  DCHECK(!params.processing_id.has_value());
  return AudioRendererMixerManager::GetInstance().CreateInput(
      frame_token, params.session_id, params.device_id,
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
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  // AudioRendererMixer sinks are always used asynchronously and thus can
  // operate without a timeout value.
  return NewFinalAudioRendererSink(frame_token, params, base::TimeDelta());
}

// static
scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  if (factory_) {
    scoped_refptr<media::AudioRendererSink> device =
        factory_->CreateAudioRendererSink(source_type, frame_token, params);
    if (device)
      return device;
  }

  // Perhaps streams with a processing ID just shouldn't be mixable, i.e. call
  // NewFinalAudioRendererSink for them rather than DCHECK?
  DCHECK(!(params.processing_id.has_value() && IsMixable(source_type)));

  if (IsMixable(source_type))
    return NewMixableSink(source_type, frame_token, params);

  UMA_HISTOGRAM_BOOLEAN("Media.Audio.Render.SinkCache.UsedForSinkCreation",
                        false);
  return NewFinalAudioRendererSink(frame_token, params,
                                   GetDefaultAuthTimeout());
}

// static
scoped_refptr<media::SwitchableAudioRendererSink>
AudioDeviceFactory::NewSwitchableAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  if (factory_) {
    scoped_refptr<media::SwitchableAudioRendererSink> sink =
        factory_->CreateSwitchableAudioRendererSink(source_type, frame_token,
                                                    params);
    if (sink)
      return sink;
  }

  if (IsMixable(source_type))
    return NewMixableSink(source_type, frame_token, params);

  // AudioOutputDevice is not RestartableAudioRendererSink, so we can't return
  // anything for those who wants to create an unmixable sink.
  NOTIMPLEMENTED();
  return nullptr;
}

// static
scoped_refptr<media::AudioCapturerSource>
AudioDeviceFactory::NewAudioCapturerSource(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSourceParameters& params) {
  if (factory_) {
    // We don't pass on |session_id|, as this branch is only used for tests.
    scoped_refptr<media::AudioCapturerSource> source =
        factory_->CreateAudioCapturerSource(frame_token, params);
    if (source)
      return source;
  }

  return base::MakeRefCounted<media::AudioInputDevice>(
      blink::WebAudioInputIPCFactory::GetInstance().CreateAudioInputIPC(
          frame_token, params),
      media::AudioInputDevice::Purpose::kUserInput,
      media::AudioInputDevice::DeadStreamDetection::kEnabled);
}

// static
media::OutputDeviceInfo AudioDeviceFactory::GetOutputDeviceInfo(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  DCHECK(IsMainThread()) << __func__ << "() is called on a wrong thread.";
  constexpr base::TimeDelta kDeleteTimeout =
      base::TimeDelta::FromMilliseconds(5000);

  // There's one process wide instance that lives on the render thread.
  //
  // TODO(crbug.com/787252): Replace the use of base::ThreadPool below by
  // worker_pool::PostTask().
  static base::NoDestructor<AudioRendererSinkCacheImpl> cache(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      base::BindRepeating(&AudioDeviceFactory::NewAudioRendererSink,
                          blink::WebAudioDeviceSourceType::kNone),
      kDeleteTimeout);
  return cache->GetSinkInfo(frame_token, params.session_id, params.device_id);
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
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  if (factory_) {
    scoped_refptr<media::AudioRendererSink> sink =
        factory_->CreateFinalAudioRendererSink(frame_token, params,
                                               auth_timeout);
    if (sink)
      return sink;
  }

  return NewOutputDevice(frame_token, params, auth_timeout);
}

}  // namespace blink
