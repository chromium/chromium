// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/public/renderer/render_frame.h"
#include "media/audio/audio_features.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/output_device_info.h"
#include "media/base/silent_sink_suspender.h"
#include "media/base/speech_recognition_client.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/task_type.h"
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

using ::media::limits::kMaxWebAudioBufferSize;
using ::media::limits::kMinWebAudioBufferSize;

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
}


media::AudioParameters GetOutputDeviceParameters(
    const blink::LocalFrameToken& frame_token,
    const std::string& device_id) {
  TRACE_EVENT0("webaudio", "GetOutputDeviceParameters");
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
    int number_of_output_channels,
    const blink::WebAudioLatencyHint& latency_hint,
    std::optional<float> context_sample_rate,
    media::AudioRendererSink::RenderCallback* callback) {
  // The `number_of_output_channels` does not manifest the actual channel
  // layout of the audio output device. We use the best guess to the channel
  // layout based on the number of channels.
  media::ChannelLayout layout =
      media::GuessChannelLayout(number_of_output_channels);

  // Use "discrete" channel layout when the best guess was not successful.
  if (layout == media::CHANNEL_LAYOUT_UNSUPPORTED) {
    layout = media::CHANNEL_LAYOUT_DISCRETE;
  }

  return std::unique_ptr<RendererWebAudioDeviceImpl>(
      new RendererWebAudioDeviceImpl(
          sink_descriptor, {layout, number_of_output_channels}, latency_hint,
          context_sample_rate, callback,
          base::BindOnce(&GetOutputDeviceParameters),
          base::BindRepeating(&GetNullAudioSink)));
}

int RendererWebAudioDeviceImpl::GetOutputBufferSize(
    const blink::WebAudioLatencyHint& latency_hint,
    int resolved_context_sample_rate,
    const media::AudioParameters& hardware_params) {
  const media::AudioParameters::HardwareCapabilities hardware_capabilities =
      hardware_params.hardware_capabilities().value_or(
          media::AudioParameters::HardwareCapabilities());

  const float scale_factor = static_cast<float>(resolved_context_sample_rate) /
                             hardware_params.sample_rate();

  int min_hardware_buffer_size = hardware_capabilities.min_frames_per_buffer;
  int max_hardware_buffer_size = hardware_capabilities.max_frames_per_buffer;

  // The hardware may not provide explicit buffer size limits. In such cases,
  // we fall back to predefined minimum and maximum buffer sizes. Additionally,
  // hardware-provided limits are defined at the hardware's default sample rate.
  // We must scale these limits to the context's sample rate, as subsequent
  // buffer size calculations rely on the context sample rate.
  int min_buffer_size = kMinWebAudioBufferSize;
  if (min_hardware_buffer_size != 0) {
    min_buffer_size = std::max(
        kMinWebAudioBufferSize,
        static_cast<int>(std::ceil(min_hardware_buffer_size * scale_factor)));
  }

  int max_buffer_size = kMaxWebAudioBufferSize;
  if (max_hardware_buffer_size != 0) {
    max_buffer_size = std::min(
        kMaxWebAudioBufferSize,
        static_cast<int>(std::ceil(max_hardware_buffer_size * scale_factor)));
  }
  // Ensure that the `min_buffer_size` does not exceed `max_buffer_size`.
  // This can occur when a small scale_factor leads to inverted limits after
  // scaling and clamping.
  max_buffer_size = std::max(min_buffer_size, max_buffer_size);

  // Scale default buffer size to context rate. buffer size calculations for
  // each latency hint now use the context rate (instead of hardware rate).
  // Scaling ensures the calculated buffer size corresponds to the desired
  // callback interval at the context rate.
  int scaled_default_buffer_size = static_cast<int>(
      std::ceil(hardware_params.frames_per_buffer() * scale_factor));

  // Clamp the scaled default buffer size to the valid range.
  scaled_default_buffer_size =
      std::clamp(scaled_default_buffer_size, min_buffer_size, max_buffer_size);

  int output_buffer_size = -1;
  switch (latency_hint.Category()) {
    case WebAudioLatencyHint::kCategoryInteractive:
      output_buffer_size = media::AudioLatency::GetInteractiveBufferSize(
          scaled_default_buffer_size);
      break;
    case WebAudioLatencyHint::kCategoryBalanced:
      output_buffer_size = media::AudioLatency::GetRtcBufferSize(
          resolved_context_sample_rate, scaled_default_buffer_size);
      break;
    case WebAudioLatencyHint::kCategoryPlayback:
      output_buffer_size = media::AudioLatency::GetHighLatencyBufferSize(
          resolved_context_sample_rate, scaled_default_buffer_size);
      break;
    case WebAudioLatencyHint::kCategoryExact:
      output_buffer_size = media::AudioLatency::GetExactBufferSize(
          base::Seconds(latency_hint.Seconds()), resolved_context_sample_rate,
          scaled_default_buffer_size, min_buffer_size, max_buffer_size,
          kMaxWebAudioBufferSize);
      break;
    case WebAudioLatencyHint::kLastValue:
      NOTREACHED();
  }

  CHECK(output_buffer_size != -1)
      << "RendererWebAudioDeviceImpl::GetOutputBufferSize: Output buffer size "
         "was not updated from initial value (-1). "
      << "Latency Hint Category: " << static_cast<int>(latency_hint.Category());

  TRACE_EVENT_INSTANT(
      "webaudio", "RendererWebAudioDeviceImpl::GetOutputBufferSize",
      "latency_hint", blink::WebAudioLatencyHint::AsString(latency_hint),
      "resolved_context_sample_rate", resolved_context_sample_rate,
      "hardware_params", hardware_params.AsHumanReadableString(),
      "scale_factor", scale_factor, "min_buffer_size", min_buffer_size,
      "max_buffer_size", max_buffer_size, "scaled_default_buffer_size",
      scaled_default_buffer_size, "output_buffer_size", output_buffer_size);

  return output_buffer_size;
}

RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl(
    const WebAudioSinkDescriptor& sink_descriptor,
    media::ChannelLayoutConfig layout_config,
    const blink::WebAudioLatencyHint& latency_hint,
    std::optional<float> context_sample_rate,
    media::AudioRendererSink::RenderCallback* callback,
    OutputDeviceParamsCallback device_params_cb,
    CreateSilentSinkCallback create_silent_sink_cb)
    : sink_descriptor_(sink_descriptor),
      latency_hint_(latency_hint),
      webaudio_callback_(callback),
      frame_token_(sink_descriptor.Token()),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
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

  original_sink_params_ =
      std::move(device_params_cb).Run(frame_token_, device_id);

  // On systems without audio hardware the returned parameters may be invalid.
  // In which case just choose whatever we want for the fake device.
  if (!original_sink_params_.IsValid()) {
    SendLogMessage(base::StringPrintf(
        "%s => (original_sink_params_ is invalid =[original_sink_params_=%s])",
        __func__, original_sink_params_.AsHumanReadableString().c_str()));
    original_sink_params_.Reset(media::AudioParameters::AUDIO_FAKE,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);

    // Inform the Blink client (e.g. AudioContext) that we have invalid device
    // parameters.
    // Post a task on the same thread, and the posted task will be executed
    // once the construction sequence is finished.
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&RendererWebAudioDeviceImpl::NotifyRenderError,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  SendLogMessage(base::StringPrintf(
      "%s => (hardware_params=[%s])", __func__,
      original_sink_params_.AsHumanReadableString().c_str()));

  // If the 'WebAudioRemoveAudioDestinationResampler' feature is enabled and
  // a context sample rate is provided, use the provided context sample rate.
  // Otherwise, fall back to the use default hardware sample rate to create
  // sink.
  int resolved_context_sample_rate;
  if (base::FeatureList::IsEnabled(
          features::kWebAudioRemoveAudioDestinationResampler) &&
      context_sample_rate.has_value()) {
    resolved_context_sample_rate = *context_sample_rate;
  } else {
    resolved_context_sample_rate = original_sink_params_.sample_rate();
  }

  const int output_buffer_size = GetOutputBufferSize(
      latency_hint_, resolved_context_sample_rate, original_sink_params_);

  DCHECK_NE(0, output_buffer_size);

  current_sink_params_.Reset(original_sink_params_.format(), layout_config,
                             resolved_context_sample_rate, output_buffer_size);

  // Specify the latency info to be passed to the browser side.
  current_sink_params_.set_latency_tag(AudioDeviceFactory::GetSourceLatencyType(
      GetLatencyHintSourceType(latency_hint_.Category())));
  SendLogMessage(
      base::StringPrintf("%s => (sink_params=[%s])", __func__,
                         current_sink_params_.AsHumanReadableString().c_str()));

  if (base::FeatureList::IsEnabled(media::kLiveCaptionWebAudio)) {
    auto* web_local_frame = WebLocalFrame::FromFrameToken(frame_token_);
    if (web_local_frame) {
      speech_recognition_client_ =
          web_local_frame->Client()->CreateSpeechRecognitionClient();
      if (speech_recognition_client_) {
        speech_recognition_client_->Reconfigure(current_sink_params_);
      }
    }
  }
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
  SendLogMessage(base::StringPrintf("%s({enable_silence_detection=%s})",
                                    __func__,
                                    base::ToString(enable_silence_detection)));
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
  // This function gets called from the audio infra, non-main thread, so this
  // posts a cross-thread task to the main thread task runner.
  main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&RendererWebAudioDeviceImpl::NotifyRenderError,
                       weak_ptr_factory_.GetWeakPtr()));
}

void RendererWebAudioDeviceImpl::NotifyRenderError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));

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
        {base::TaskPriority::USER_BLOCKING,
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
RendererWebAudioDeviceImpl::MaybeCreateSinkAndGetStatus() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!sink_) {
    CreateAudioRendererSink();
  }

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
