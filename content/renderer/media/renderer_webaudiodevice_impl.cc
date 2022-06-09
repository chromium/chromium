// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/silent_sink_suspender.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using blink::AudioDeviceFactory;
using blink::WebAudioDevice;
using blink::WebAudioLatencyHint;
using blink::WebLocalFrame;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

blink::WebAudioDeviceSourceType GetLatencyHintSourceType(
    WebAudioLatencyHint::AudioContextLatencyCategory latency_category) {
  switch (latency_category) {
    case WebAudioLatencyHint::kCategoryInteractive:
      return blink::WebAudioDeviceSourceType::kWebAudioInteractive;
    case WebAudioLatencyHint::kCategoryBalanced:
      return blink::WebAudioDeviceSourceType::kWebAudioBalanced;
    case WebAudioLatencyHint::kCategoryPlayback:
      return blink::WebAudioDeviceSourceType::kWebAudioPlayback;
    case WebAudioLatencyHint::kCategoryExact:
      return blink::WebAudioDeviceSourceType::kWebAudioExact;
    case WebAudioLatencyHint::kLastValue:
      NOTREACHED();
  }
  NOTREACHED();
  return blink::WebAudioDeviceSourceType::kWebAudioInteractive;
}

int GetOutputBufferSize(const blink::WebAudioLatencyHint& latency_hint,
                        media::AudioLatency::LatencyType latency,
                        const media::AudioParameters& hardware_params) {
  media::AudioParameters::HardwareCapabilities hardware_capabilities =
      hardware_params.hardware_capabilities().value_or(
          media::AudioParameters::HardwareCapabilities());

  // Adjust output buffer size according to the latency requirement.
  switch (latency) {
    case media::AudioLatency::LATENCY_INTERACTIVE:
      return media::AudioLatency::GetInteractiveBufferSize(
          hardware_params.frames_per_buffer());
    case media::AudioLatency::LATENCY_RTC:
      return media::AudioLatency::GetRtcBufferSize(
          hardware_params.sample_rate(), hardware_params.frames_per_buffer());
    case media::AudioLatency::LATENCY_PLAYBACK:
      return media::AudioLatency::GetHighLatencyBufferSize(
          hardware_params.sample_rate(), hardware_params.frames_per_buffer());
    case media::AudioLatency::LATENCY_EXACT_MS:
      return media::AudioLatency::GetExactBufferSize(
          base::Seconds(latency_hint.Seconds()), hardware_params.sample_rate(),
          hardware_params.frames_per_buffer(),
          hardware_capabilities.min_frames_per_buffer,
          hardware_capabilities.max_frames_per_buffer,
          media::limits::kMaxWebAudioBufferSize);
    default:
      NOTREACHED();
  }
  return 0;
}

blink::LocalFrameToken FrameTokenFromCurrentContext() {
  // TODO(crbug.com/1307461): The assumption here is incorrect;
  // RendererWebAudioDevice can be created without a valid frame/document. In
  // that case, FrameForCurrentContext() below will be invalid.

  // We can perform look-ups to determine which RenderView is starting the
  // audio device.  The reason for all this is because the creator of the
  // WebAudio objects might not be the actual source of the audio (e.g.,
  // an extension creates a object that is passed and used within a page).
  return blink::WebLocalFrame::FrameForCurrentContext()->GetLocalFrameToken();
}

media::AudioParameters GetOutputDeviceParameters(
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& session_id,
    const std::string& device_id) {
  return AudioDeviceFactory::GetInstance()
      ->GetOutputDeviceInfo(frame_token, {session_id, device_id})
      .output_params();
}

}  // namespace

std::unique_ptr<RendererWebAudioDeviceImpl> RendererWebAudioDeviceImpl::Create(
    media::ChannelLayout layout,
    int channels,
    const blink::WebAudioLatencyHint& latency_hint,
    WebAudioDevice::RenderCallback* callback,
    const base::UnguessableToken& session_id) {
  return std::unique_ptr<RendererWebAudioDeviceImpl>(
      new RendererWebAudioDeviceImpl(
          layout, channels, latency_hint, callback, session_id,
          base::BindOnce(&GetOutputDeviceParameters),
          base::BindOnce(&FrameTokenFromCurrentContext)));
}

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    media::ChannelLayout layout,
    int channels,
    const blink::WebAudioLatencyHint& latency_hint,
    WebAudioDevice::RenderCallback* callback,
    const base::UnguessableToken& session_id,
    OutputDeviceParamsCallback device_params_cb,
    RenderFrameTokenCallback render_frame_token_cb)
    : latency_hint_(latency_hint),
      client_callback_(callback),
      session_id_(session_id),
      frame_token_(std::move(render_frame_token_cb).Run()) {
  DCHECK(client_callback_);
  SendLogMessage(base::StringPrintf("%s", __func__));

  media::AudioParameters hardware_params(
      std::move(device_params_cb)
          .Run(frame_token_, session_id_, std::string()));

  // On systems without audio hardware the returned parameters may be invalid.
  // In which case just choose whatever we want for the fake device.
  if (!hardware_params.IsValid()) {
    hardware_params.Reset(media::AudioParameters::AUDIO_FAKE,
                          media::CHANNEL_LAYOUT_STEREO, 48000, 480);
  }
  SendLogMessage(
      base::StringPrintf("%s => (hardware_params=[%s])", __func__,
                         hardware_params.AsHumanReadableString().c_str()));

  const media::AudioLatency::LatencyType latency =
      AudioDeviceFactory::GetSourceLatencyType(
          GetLatencyHintSourceType(latency_hint_.Category()));

  const int output_buffer_size =
      GetOutputBufferSize(latency_hint_, latency, hardware_params);
  DCHECK_NE(0, output_buffer_size);

  sink_params_.Reset(hardware_params.format(), layout,
                     hardware_params.sample_rate(), output_buffer_size);

  // Always set channels, this should be a no-op in all but the discrete case;
  // this call will fail if channels doesn't match the layout in other cases.
  sink_params_.set_channels_for_discrete(channels);

  // Specify the latency info to be passed to the browser side.
  sink_params_.set_latency_tag(latency);
  SendLogMessage(
      base::StringPrintf("%s => (sink_params=[%s])", __func__,
                         sink_params_.AsHumanReadableString().c_str()));

  web_audio_dest_data_ =
      blink::WebVector<float*>(static_cast<size_t>(sink_params_.channels()));
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  DCHECK(!sink_);
}

void RendererWebAudioDeviceImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));

  if (sink_)
    return;  // Already started.

  sink_ = AudioDeviceFactory::GetInstance()->NewAudioRendererSink(
      GetLatencyHintSourceType(latency_hint_.Category()), frame_token_,
      media::AudioSinkParameters(session_id_, std::string()));

  // Use a task runner instead of the render thread for fake Render() calls
  // since it has special connotations for Blink and garbage collection. Timeout
  // value chosen to be highly unlikely in the normal case.
  webaudio_suspender_ = std::make_unique<media::SilentSinkSuspender>(
      this, base::Seconds(30), sink_params_, sink_, GetSuspenderTaskRunner());
  sink_->Initialize(sink_params_, webaudio_suspender_.get());

  sink_->Start();
  sink_->Play();
}

void RendererWebAudioDeviceImpl::Pause() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));
  if (sink_)
    sink_->Pause();
  if (webaudio_suspender_)
    webaudio_suspender_->OnPaused();
}

void RendererWebAudioDeviceImpl::Resume() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));
  if (sink_)
    sink_->Play();
}

void RendererWebAudioDeviceImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));
  if (sink_) {
    sink_->Stop();
    sink_ = nullptr;
  }

  webaudio_suspender_.reset();
}

double RendererWebAudioDeviceImpl::SampleRate() {
  return sink_params_.sample_rate();
}

int RendererWebAudioDeviceImpl::FramesPerBuffer() {
  return sink_params_.frames_per_buffer();
}

void RendererWebAudioDeviceImpl::SetDetectSilence(
    bool enable_silence_detection) {
  SendLogMessage(
      base::StringPrintf("%s({enable_silence_detection=%s})", __func__,
                         enable_silence_detection ? "true" : "false"));
  DCHECK(thread_checker_.CalledOnValidThread());

  if (webaudio_suspender_)
    webaudio_suspender_->SetDetectSilence(enable_silence_detection);
}

int RendererWebAudioDeviceImpl::Render(base::TimeDelta delay,
                                       base::TimeTicks delay_timestamp,
                                       int prior_frames_skipped,
                                       media::AudioBus* dest) {
  // Wrap the output pointers using WebVector.
  CHECK_EQ(dest->channels(), sink_params_.channels());
  for (int i = 0; i < dest->channels(); ++i)
    web_audio_dest_data_[i] = dest->channel(i);

  client_callback_->Render(
      web_audio_dest_data_, dest->frames(), delay.InSecondsF(),
      (delay_timestamp - base::TimeTicks()).InSecondsF(), prior_frames_skipped);

  if (!is_rendering_) {
    SendLogMessage(base::StringPrintf("%s => (rendering is alive [frames=%d])",
                                      __func__, dest->frames()));
    is_rendering_ = true;
  }

  return dest->frames();
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  // TODO(crogers): implement error handling.
}

void RendererWebAudioDeviceImpl::SetSuspenderTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  suspender_task_runner_ = std::move(task_runner);
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererWebAudioDeviceImpl::GetSuspenderTaskRunner() {
  if (!suspender_task_runner_) {
    suspender_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }
  return suspender_task_runner_;
}

void RendererWebAudioDeviceImpl::SendLogMessage(const std::string& message) {
  if (session_id_.is_empty()) {
    blink::WebRtcLogMessage(
        base::StringPrintf("[WA]RWADI::%s", message.c_str()));
  } else {
    blink::WebRtcLogMessage(base::StringPrintf("[WA]RWADI::%s [session_id=%s]",
                                               message.c_str(),
                                               session_id_.ToString().c_str()));
  }
}

}  // namespace content
