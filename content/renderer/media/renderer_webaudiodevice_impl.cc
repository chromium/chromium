// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/output_device_info.h"
#include "media/base/silent_sink_suspender.h"
#include "media/base/speech_recognition_client.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
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
      NOTREACHED_NORETURN();
  }
  NOTREACHED_NORETURN();
}

int GetOutputBufferSize(
    const blink::WebAudioLatencyHint& latency_hint,
    int sample_rate,
    int device_frames_per_buffer,
    media::AudioParameters::HardwareCapabilities hardware_capabilities) {
  // Adjust output buffer size according to the latency requirement.
  switch (latency_hint.Category()) {
    case WebAudioLatencyHint::kCategoryInteractive:
      return media::AudioLatency::GetInteractiveBufferSize(
          device_frames_per_buffer);
    case WebAudioLatencyHint::kCategoryBalanced:
      return media::AudioLatency::GetRtcBufferSize(sample_rate,
                                                   device_frames_per_buffer);
    case WebAudioLatencyHint::kCategoryPlayback:
      return media::AudioLatency::GetHighLatencyBufferSize(
          sample_rate, device_frames_per_buffer);
    case WebAudioLatencyHint::kCategoryExact:
      return media::AudioLatency::GetExactBufferSize(
          base::Seconds(latency_hint.Seconds()), sample_rate,
          device_frames_per_buffer, hardware_capabilities.min_frames_per_buffer,
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
  TRACE_EVENT0("webaudio", "GetOutputDeviceParameters");
  return AudioDeviceFactory::GetInstance()
      ->GetOutputDeviceInfo(frame_token, device_id)
      .output_params();
}

void ReportUma(const media::AudioParameters& device_params,
               const media::AudioParameters& sink_params,
               bool sample_rate_provided) {
  base::UmaHistogramSparse("WebAudio.AudioDestination.HardwareBufferSize",
                           device_params.frames_per_buffer());

  // The actual callback size used.
  base::UmaHistogramSparse("WebAudio.AudioDestination.CallbackBufferSize",
                           sink_params.frames_per_buffer());

  base::UmaHistogramSparse("WebAudio.AudioContext.HardwareSampleRate",
                           device_params.sample_rate());

  // Record the selected sample rate and ratio if the sample rate was given. The
  // ratio is recorded as a percentage, rounded to the nearest percent.
  if (sample_rate_provided) {
    // The actual supplied `context_sample_rate` is probably a small set
    // including 44100, 48000, 22050, and 2400 Hz.  Other valid values range
    // from 3000 to 384000 Hz, but are not expected to be used much.
    base::UmaHistogramSparse("WebAudio.AudioContextOptions.sampleRate",
                             sink_params.sample_rate());

    int32_t scale_factor = static_cast<int32_t>(
        (sink_params.sample_rate() * 100 + 0.5) / device_params.sample_rate());

    // From the expected values above and the common HW sample rates, we expect
    // the most common ratios to be the set 0.5, 44100/48000, and 48000/44100.
    // Other values are possible but seem unlikely.
    base::UmaHistogramSparse("WebAudio.AudioContextOptions.sampleRateRatio",
                             scale_factor);
  }
}

scoped_refptr<media::AudioRendererSink> GetNullAudioSink(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  return base::MakeRefCounted<media::NullAudioSink>(task_runner);
}

}  // namespace

std::unique_ptr<RendererWebAudioDeviceImpl> RendererWebAudioDeviceImpl::Create(
    const WebAudioSinkDescriptor& sink_descriptor,
    media::ChannelLayoutConfig channel_layout_config,
    const blink::WebAudioLatencyHint& latency_hint,
    std::optional<float> sample_rate,
    media::AudioRendererSink::RenderCallback* callback) {
  return std::unique_ptr<RendererWebAudioDeviceImpl>(
      new RendererWebAudioDeviceImpl(sink_descriptor, channel_layout_config,
                                     latency_hint, sample_rate, callback,
                                     base::BindOnce(&GetOutputDeviceParameters),
                                     base::BindRepeating(&GetNullAudioSink)));
}

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    const WebAudioSinkDescriptor& sink_descriptor,
    media::ChannelLayoutConfig channel_layout_config,
    const blink::WebAudioLatencyHint& latency_hint,
    std::optional<float> sample_rate,
    media::AudioRendererSink::RenderCallback* callback,
    OutputDeviceParamsCallback device_params_cb,
    CreateSilentSinkCallback create_silent_sink_cb)
    : sink_descriptor_(sink_descriptor),
      latency_hint_(latency_hint),
      webaudio_callback_(callback),
      frame_token_(sink_descriptor.Token()),
      create_silent_sink_cb_(std::move(create_silent_sink_cb)) {
  TRACE_EVENT0("webaudio",
               "RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl");
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

  media::AudioParameters device_params =
      std::move(device_params_cb).Run(frame_token_, device_id);

  // On systems without audio hardware the returned parameters may be invalid.
  // In which case just choose whatever we want for the fake device.
  if (!device_params.IsValid()) {
    // TODO(https://crbug.com/1522759): Bubble up this sink failure to the JS
    // API surface.
    device_params.Reset(media::AudioParameters::AUDIO_FAKE,
                        media::ChannelLayoutConfig::Stereo(), 48000, 480);
  }
  SendLogMessage(
      base::StringPrintf("%s => (hardware_params=[%s])", __func__,
                         device_params.AsHumanReadableString().c_str()));

  max_channel_count_ = device_params.channels();

  const int sink_sample_rate =
      sample_rate ? *sample_rate : device_params.sample_rate();

  const int output_buffer_size = GetOutputBufferSize(
      latency_hint_, sink_sample_rate, device_params.frames_per_buffer(),
      device_params.hardware_capabilities().value_or(
          media::AudioParameters::HardwareCapabilities()));

  sink_params_.Reset(device_params.format(), channel_layout_config,
                     sink_sample_rate, output_buffer_size);

  // Specify the latency info to be passed to the browser side.
  sink_params_.set_latency_tag(AudioDeviceFactory::GetSourceLatencyType(
      GetLatencyHintSourceType(latency_hint_.Category())));

  CHECK(sink_params_.IsValid());

  SendLogMessage(
      base::StringPrintf("%s => (sink_params=[%s])", __func__,
                         sink_params_.AsHumanReadableString().c_str()));

  if (base::FeatureList::IsEnabled(media::kLiveCaptionWebAudio)) {
    auto* web_local_frame = WebLocalFrame::FromFrameToken(frame_token_);
    if (web_local_frame) {
      speech_recognition_client_ =
          web_local_frame->Client()->CreateSpeechRecognitionClient();
      if (speech_recognition_client_) {
        speech_recognition_client_->Reconfigure(sink_params_);
      }
    }
  }

  ReportUma(device_params, sink_params_, sample_rate.has_value());
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  // In case device is not stopped, we can stop it here.
  Stop();
}

void RendererWebAudioDeviceImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));

  // Already started.
  if (!is_stopped_) {
    return;
  }

  if (!sink_) {
    CreateAudioRendererSink();
  }

  sink_->Start();
  sink_->Play();
  is_stopped_ = false;
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
  is_stopped_ = true;
}

double RendererWebAudioDeviceImpl::SampleRate() {
  return sink_params_.sample_rate();
}

int RendererWebAudioDeviceImpl::FramesPerBuffer() {
  return sink_params_.frames_per_buffer();
}

int RendererWebAudioDeviceImpl::MaxChannelCount() {
  return max_channel_count_;
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

  int frames_filled =
      webaudio_callback_->Render(delay, delay_timestamp, glitch_info, dest);
  if (speech_recognition_client_) {
    speech_recognition_client_->AddAudio(*dest);
  }

  return frames_filled;
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
  TRACE_EVENT0("webaudio",
               "RendererWebAudioDeviceImpl::CreateAudioRendererSink");
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!sink_);

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
          this, base::Seconds(30), sink_params_, sink_,
          GetSilentSinkTaskRunner());
      sink_->Initialize(sink_params_, silent_sink_suspender_.get());
      break;
    case blink::WebAudioSinkDescriptor::kSilent:
      sink_ = create_silent_sink_cb_.Run(GetSilentSinkTaskRunner());
      sink_->Initialize(sink_params_, this);
      break;
  }
}

media::OutputDeviceStatus
RendererWebAudioDeviceImpl::CreateSinkAndGetDeviceStatus() {
  DCHECK(thread_checker_.CalledOnValidThread());
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
    Stop();
  }
  return status;
}

}  // namespace content
