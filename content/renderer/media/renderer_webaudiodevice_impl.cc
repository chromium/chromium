// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/output_device_info.h"
#include "media/base/silent_sink_suspender.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using blink::AudioDeviceFactory;
using blink::WebAudioDevice;
using blink::WebAudioLatencyHint;
using blink::WebAudioSinkDescriptor;
using blink::WebLocalFrame;
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

media::AudioParameters GetOutputDeviceParameters(
    const blink::LocalFrameToken& frame_token,
    const std::string& device_id) {
  return AudioDeviceFactory::GetInstance()
      ->GetOutputDeviceInfo(frame_token, device_id)
      .output_params();
}

scoped_refptr<media::AudioRendererSink> GetNullAudioSink(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  return base::MakeRefCounted<media::NullAudioSink>(task_runner);
}

}  // namespace

std::unique_ptr<RendererWebAudioDeviceImpl> RendererWebAudioDeviceImpl::Create(
    const WebAudioSinkDescriptor& sink_descriptor,
    media::ChannelLayout layout,
    int number_of_output_channels,
    const blink::WebAudioLatencyHint& latency_hint,
    media::AudioRendererSink::RenderCallback* callback) {
  return std::unique_ptr<RendererWebAudioDeviceImpl>(
      new RendererWebAudioDeviceImpl(
          sink_descriptor, layout, number_of_output_channels, latency_hint,
          callback, base::BindOnce(&GetOutputDeviceParameters),
          base::BindRepeating(&GetNullAudioSink)));
}

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    const WebAudioSinkDescriptor& sink_descriptor,
    media::ChannelLayout layout,
    int number_of_output_channels,
    const blink::WebAudioLatencyHint& latency_hint,
    media::AudioRendererSink::RenderCallback* callback,
    OutputDeviceParamsCallback device_params_cb,
    CreateSilentSinkCallback create_silent_sink_cb)
    : sink_descriptor_(sink_descriptor),
      latency_hint_(latency_hint),
      webaudio_callback_(callback),
      frame_token_(sink_descriptor.Token()),
      create_silent_sink_cb_(std::move(create_silent_sink_cb)) {
  DCHECK(webaudio_callback_);
  SendLogMessage(base::StringPrintf("%s", __func__));

  std::string device_id;
  switch (sink_descriptor_.Type()) {
    case blink::WebAudioSinkDescriptor::kAudible:
      device_id = sink_descriptor_.SinkId().Utf8();
      break;
    case blink::WebAudioSinkDescriptor::kSilent:
      // Use the default audio device's parameters for a silent sink.
      device_id = std::string();
      break;
  }

  original_sink_params_ =
      std::move(device_params_cb).Run(frame_token_, device_id);

  // On systems without audio hardware the returned parameters may be invalid.
  // In which case just choose whatever we want for the fake device.
  if (!original_sink_params_.IsValid()) {
    original_sink_params_.Reset(media::AudioParameters::AUDIO_FAKE,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
  }
  SendLogMessage(base::StringPrintf(
      "%s => (hardware_params=[%s])", __func__,
      original_sink_params_.AsHumanReadableString().c_str()));

  const media::AudioLatency::LatencyType latency =
      AudioDeviceFactory::GetSourceLatencyType(
          GetLatencyHintSourceType(latency_hint_.Category()));

  const int output_buffer_size =
      GetOutputBufferSize(latency_hint_, latency, original_sink_params_);
  DCHECK_NE(0, output_buffer_size);

  current_sink_params_.Reset(
      original_sink_params_.format(), {layout, number_of_output_channels},
      original_sink_params_.sample_rate(), output_buffer_size);

  // Specify the latency info to be passed to the browser side.
  current_sink_params_.set_latency_tag(latency);
  SendLogMessage(
      base::StringPrintf("%s => (sink_params=[%s])", __func__,
                         current_sink_params_.AsHumanReadableString().c_str()));
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  DCHECK(!sink_);
}

void RendererWebAudioDeviceImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));

  if (sink_)
    return;  // Already started.

  CreateAudioRendererSink();
  sink_->Start();
  sink_->Play();
}

void RendererWebAudioDeviceImpl::Pause() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));
  if (sink_)
    sink_->Pause();
  if (silent_sink_suspender_)
    silent_sink_suspender_->OnPaused();
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

  silent_sink_suspender_.reset();
}

double RendererWebAudioDeviceImpl::SampleRate() {
  return current_sink_params_.sample_rate();
}

int RendererWebAudioDeviceImpl::FramesPerBuffer() {
  return current_sink_params_.frames_per_buffer();
}

int RendererWebAudioDeviceImpl::MaxChannelCount() {
  return original_sink_params_.channels();
}

void RendererWebAudioDeviceImpl::SetDetectSilence(
    bool enable_silence_detection) {
  SendLogMessage(
      base::StringPrintf("%s({enable_silence_detection=%s})", __func__,
                         enable_silence_detection ? "true" : "false"));
  DCHECK(thread_checker_.CalledOnValidThread());

  if (silent_sink_suspender_)
    silent_sink_suspender_->SetDetectSilence(enable_silence_detection);
}

int RendererWebAudioDeviceImpl::Render(
    base::TimeDelta delay,
    base::TimeTicks delay_timestamp,
    const media::AudioGlitchInfo& glitch_info,
    media::AudioBus* dest) {
  if (!is_rendering_) {
    SendLogMessage(base::StringPrintf("%s => (rendering is alive [frames=%d])",
                                      __func__, dest->frames()));
    is_rendering_ = true;
  }

  return webaudio_callback_->Render(delay, delay_timestamp, glitch_info, dest);
}

void RendererWebAudioDeviceImpl::OnRenderError() {
  DCHECK(webaudio_callback_);

  webaudio_callback_->OnRenderError();
}

void RendererWebAudioDeviceImpl::SetSilentSinkTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  silent_sink_task_runner_ = std::move(task_runner);
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererWebAudioDeviceImpl::GetSilentSinkTaskRunner() {
  if (!silent_sink_task_runner_) {
    silent_sink_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  }
  return silent_sink_task_runner_;
}

void RendererWebAudioDeviceImpl::SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage(base::StringPrintf("[WA]RWADI::%s", message.c_str()));
}

void RendererWebAudioDeviceImpl::CreateAudioRendererSink() {
  DCHECK(!sink_);

  switch (sink_descriptor_.Type()) {
    case blink::WebAudioSinkDescriptor::kAudible:
      sink_ = AudioDeviceFactory::GetInstance()->NewAudioRendererSink(
          GetLatencyHintSourceType(latency_hint_.Category()), frame_token_,
          media::AudioSinkParameters(base::UnguessableToken(),
                                     sink_descriptor_.SinkId().Utf8()));

      // Use a task runner instead of the render thread for fake Render() calls
      // since it has special connotations for Blink and garbage collection.
      // Timeout value chosen to be highly unlikely in the normal case.
      silent_sink_suspender_ = std::make_unique<media::SilentSinkSuspender>(
          this, base::Seconds(30), current_sink_params_, sink_,
          GetSilentSinkTaskRunner());
      sink_->Initialize(current_sink_params_, silent_sink_suspender_.get());
      break;
    case blink::WebAudioSinkDescriptor::kSilent:
      sink_ = create_silent_sink_cb_.Run(GetSilentSinkTaskRunner());
      sink_->Initialize(current_sink_params_, this);
      break;
  }
}

media::OutputDeviceStatus
RendererWebAudioDeviceImpl::CreateSinkAndGetDeviceStatus() {
  CreateAudioRendererSink();

  // The device status of a silent sink is always OK.
  bool is_silent_sink =
      sink_descriptor_.Type() == blink::WebAudioSinkDescriptor::kSilent;
  media::OutputDeviceStatus status =
      is_silent_sink ? media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK
                     : sink_->GetOutputDeviceInfo().device_status();

  // If sink status is not OK, reset `sink_` and `silent_sink_suspender_`
  // because this instance will be destroyed.
  if (status != media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK) {
    sink_->Stop();
    sink_ = nullptr;
    silent_sink_suspender_.reset();
  } else {
    sink_->Start();
    sink_->Play();
  }
  return status;
}

}  // namespace content
