// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/audio/audio_input_device.h"
#include "media/audio/audio_output_device.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/modules/media/audio/audio_input_ipc_factory.h"
#include "third_party/blink/public/web/modules/media/audio/audio_output_ipc_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_input.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_mixer_manager.h"
#include "third_party/blink/renderer/modules/media/audio/audio_renderer_sink_cache.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

// Set when the default factory is overridden.
AudioDeviceFactory* g_factory_override = nullptr;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
// Due to driver deadlock issues on Windows (http://crbug/422522) there is a
// chance device authorization response is never received from the browser side.
// In this case we will time out, to avoid renderer hang forever waiting for
// device authorization (http://crbug/615589). This will result in "no audio".
// There are also cases when authorization takes too long on Mac and Linux.
constexpr base::TimeDelta kMaxAuthorizationTimeout = base::Seconds(10);
#else
constexpr base::TimeDelta kMaxAuthorizationTimeout;  // No timeout.
#endif

base::TimeDelta GetDefaultAuthTimeout() {
  // Set authorization request timeout at 80% of renderer hung timeout,
  // but no more than kMaxAuthorizationTimeout.
  return std::min(Platform::Current()->GetHungRendererDelay() * 8 / 10,
                  kMaxAuthorizationTimeout);
}

// Creates an output device in the rendering pipeline, `auth_timeout` is the
// authorization timeout allowed for the underlying AudioOutputDevice instance;
// a timeout of zero means no timeout.
scoped_refptr<media::AudioOutputDevice> NewOutputDevice(
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params,
    base::TimeDelta auth_timeout) {
  CHECK(blink::AudioOutputIPCFactory::GetInstance().io_task_runner());
  auto device = base::MakeRefCounted<media::AudioOutputDevice>(
      blink::AudioOutputIPCFactory::GetInstance().CreateAudioOutputIPC(
          frame_token),
      blink::AudioOutputIPCFactory::GetInstance().io_task_runner(), params,
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

}  // namespace

// static
AudioDeviceFactory* AudioDeviceFactory::GetInstance() {
  if (g_factory_override) {
    return g_factory_override;
  }

  static base::NoDestructor<AudioDeviceFactory> g_default_factory(
      /*override_default=*/false);
  return g_default_factory.get();
}

AudioDeviceFactory::AudioDeviceFactory(bool override_default) {
  if (override_default) {
    DCHECK(!g_factory_override) << "Can't register two factories at once.";
    g_factory_override = this;
  }
}

AudioDeviceFactory::~AudioDeviceFactory() {
  DCHECK_EQ(g_factory_override, this);
  g_factory_override = nullptr;
}

// static
media::AudioLatency::Type AudioDeviceFactory::GetSourceLatencyType(
    blink::WebAudioDeviceSourceType source) {
  switch (source) {
    case blink::WebAudioDeviceSourceType::kWebAudioInteractive:
      return media::AudioLatency::Type::kInteractive;
    case blink::WebAudioDeviceSourceType::kNone:
    case blink::WebAudioDeviceSourceType::kWebRtc:
    case blink::WebAudioDeviceSourceType::kNonRtcAudioTrack:
    case blink::WebAudioDeviceSourceType::kWebAudioBalanced:
      return media::AudioLatency::Type::kRtc;
    case blink::WebAudioDeviceSourceType::kMediaElement:
    case blink::WebAudioDeviceSourceType::kWebAudioPlayback:
      return media::AudioLatency::Type::kPlayback;
    case blink::WebAudioDeviceSourceType::kWebAudioExact:
      return media::AudioLatency::Type::kExactMS;
  }
  NOTREACHED_IN_MIGRATION();
  return media::AudioLatency::Type::kUnknown;
}

scoped_refptr<media::AudioRendererSink>
AudioDeviceFactory::NewAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  DCHECK(!IsMixable(source_type));
  return NewOutputDevice(frame_token, params, GetDefaultAuthTimeout());
}

scoped_refptr<media::SwitchableAudioRendererSink>
AudioDeviceFactory::NewMixableSink(blink::WebAudioDeviceSourceType source_type,
                                   const blink::LocalFrameToken& frame_token,
                                   const blink::FrameToken& main_frame_token,
                                   const media::AudioSinkParameters& params) {
  DCHECK(IsMixable(source_type));
  DCHECK(IsMainThread()) << __func__ << "() is called on a wrong thread.";
  if (!mixer_manager_) {
    auto create_sink_cb =
        base::BindRepeating([](const LocalFrameToken& frame_token,
                               const media::AudioSinkParameters& params)
                                -> scoped_refptr<media::AudioRendererSink> {
          // AudioRendererMixer sinks are always used asynchronously and thus
          // can operate without an authorization timeout value.
          return NewOutputDevice(frame_token, params, base::TimeDelta());
        });
    mixer_manager_ =
        std::make_unique<AudioRendererMixerManager>(std::move(create_sink_cb));
  }
  return mixer_manager_->CreateInput(
      frame_token, main_frame_token, params.session_id, params.device_id,
      AudioDeviceFactory::GetSourceLatencyType(source_type));
}

scoped_refptr<media::AudioCapturerSource>
AudioDeviceFactory::NewAudioCapturerSource(
    WebLocalFrame* web_frame,
    const media::AudioSourceParameters& params) {
  return base::MakeRefCounted<media::AudioInputDevice>(
      blink::AudioInputIPCFactory::CreateAudioInputIPC(
          web_frame->GetLocalFrameToken(),
          web_frame->GetTaskRunner(TaskType::kInternalMedia), params),
      media::AudioInputDevice::Purpose::kUserInput,
      media::AudioInputDevice::DeadStreamDetection::kEnabled);
}

media::OutputDeviceInfo AudioDeviceFactory::GetOutputDeviceInfo(
    const blink::LocalFrameToken& frame_token,
    const std::string& device_id) {
  DCHECK(IsMainThread()) << __func__ << "() is called on a wrong thread.";

  if (!sink_cache_) {
    auto create_sink_cb = base::BindRepeating(
        [](AudioDeviceFactory* factory, const LocalFrameToken& frame_token,
           const std::string& device_id)
            -> scoped_refptr<media::AudioRendererSink> {
          // Note: This shouldn't use NewOutputDevice directly since tests
          // override NewAudioRendererSink().
          return factory->NewAudioRendererSink(
              blink::WebAudioDeviceSourceType::kNone, frame_token,
              media::AudioSinkParameters(base::UnguessableToken(), device_id));
        },
        base::Unretained(this));

    constexpr base::TimeDelta kDeleteTimeout = base::Milliseconds(5000);
    sink_cache_ = std::make_unique<AudioRendererSinkCache>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
             base::MayBlock()}),
        std::move(create_sink_cb), kDeleteTimeout);
  }

  return sink_cache_->GetSinkInfo(frame_token, device_id);
}

}  // namespace blink
