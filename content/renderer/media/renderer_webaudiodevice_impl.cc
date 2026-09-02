// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webaudiodevice_impl.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_features.h"
#include "media/audio/null_audio_sink.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_latency.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/output_device_info.h"
#include "media/base/silent_sink_suspender.h"
#include "media/base/speech_recognition_client.h"
#include "third_party/blink/public/platform/audio/web_audio_device_source_type.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"

using blink::AudioDeviceFactory;
using blink::WebAudioLatencyHint;
using blink::WebAudioSinkDescriptor;
using blink::WebLocalFrame;

namespace content {

namespace {

using ::media::limits::kMaxWebAudioBufferSize;
using ::media::limits::kMinWebAudioBufferSize;

media::AudioParameters GetFallbackAudioParameters() {
  return media::AudioParameters(media::AudioParameters::AUDIO_FAKE,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                480);
}

media::AudioParameters GetSilentSinkAudioParameters(
    const media::ChannelLayoutConfig& layout_config,
    std::optional<float> context_sample_rate) {
  int sample_rate = 48000;
  if (context_sample_rate.has_value() && std::isfinite(*context_sample_rate) &&
      *context_sample_rate >= media::limits::kMinSampleRate &&
      *context_sample_rate <= media::limits::kMaxSampleRate) {
    sample_rate = static_cast<int>(std::round(*context_sample_rate));
  }

  media::ChannelLayoutConfig layout = layout_config;
  if (layout.channels() <= 0 ||
      layout.channels() > media::limits::kMaxChannels ||
      (layout.channel_layout() <= media::CHANNEL_LAYOUT_UNSUPPORTED &&
       layout.channel_layout() != media::CHANNEL_LAYOUT_DISCRETE)) {
    layout = media::ChannelLayoutConfig::Stereo();
  }

  const int frames_per_buffer = sample_rate / 100;
  return media::AudioParameters(media::AudioParameters::AUDIO_FAKE, layout,
                                sample_rate, frames_per_buffer);
}

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForFrame(
    const blink::LocalFrameToken& frame_token) {
  auto* web_local_frame = WebLocalFrame::FromFrameToken(frame_token);
  if (web_local_frame) {
    return web_local_frame->GetTaskRunner(blink::TaskType::kInternalMedia);
  }
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

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

  return base::WrapUnique(new RendererWebAudioDeviceImpl(
      sink_descriptor, {layout, number_of_output_channels}, latency_hint,
      context_sample_rate, callback,
      GetTaskRunnerForFrame(sink_descriptor.Token()),
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

  // Scale default buffer size to context rate. Buffer size calculations for
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
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    CreateSilentSinkCallback create_silent_sink_cb,
    scoped_refptr<base::SingleThreadTaskRunner> silent_sink_task_runner)
    : sink_descriptor_(sink_descriptor),
      latency_hint_(latency_hint),
      webaudio_callback_(callback),
      silent_sink_task_runner_(std::move(silent_sink_task_runner)),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      create_silent_sink_cb_(std::move(create_silent_sink_cb)),
      layout_config_(layout_config),
      context_sample_rate_(context_sample_rate) {
  TRACE_EVENT0("webaudio",
               "RendererWebAudioDeviceImpl::RendererWebAudioDeviceImpl");
  DCHECK(webaudio_callback_);
  CHECK(main_thread_task_runner_);
  render_error_callback_ = base::BindPostTask(
      main_thread_task_runner_,
      base::BindRepeating(&RendererWebAudioDeviceImpl::NotifyRenderError,
                          weak_ptr_factory_.GetWeakPtr()));
  SendLogMessage(base::StringPrintf("%s", __func__));

  CreateAudioRendererSink();
  HandleDeviceStatus(GetSinkOutputDeviceInfo());
}

media::OutputDeviceInfo RendererWebAudioDeviceImpl::GetSinkOutputDeviceInfo() {
  if (sink_descriptor_.Type() == blink::WebAudioSinkDescriptor::kSilent) {
    return media::OutputDeviceInfo(
        sink_descriptor_.SinkId().Utf8(), media::OUTPUT_DEVICE_STATUS_OK,
        GetSilentSinkAudioParameters(layout_config_, context_sample_rate_));
  }
  return sink_->GetOutputDeviceInfo();
}

void RendererWebAudioDeviceImpl::HandleDeviceStatus(
    media::OutputDeviceInfo device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  media::OutputDeviceStatus status = device_info.device_status();
  original_sink_params_ = device_info.output_params();

  // On systems without audio hardware the returned parameters may be invalid.
  // In which case just choose whatever we want for the fake device.
  if (!original_sink_params_.IsValid()) {
    SendLogMessage(base::StringPrintf(
        "%s => (original_sink_params_ is invalid =[original_sink_params_=%s])",
        __func__, original_sink_params_.AsHumanReadableString().c_str()));
    original_sink_params_ = GetFallbackAudioParameters();

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
      context_sample_rate_.has_value()) {
    resolved_context_sample_rate = *context_sample_rate_;
  } else {
    resolved_context_sample_rate = original_sink_params_.sample_rate();
  }

  const int output_buffer_size = GetOutputBufferSize(
      latency_hint_, resolved_context_sample_rate, original_sink_params_);

  DCHECK_NE(0, output_buffer_size);

  current_sink_params_.Reset(original_sink_params_.format(), layout_config_,
                             resolved_context_sample_rate, output_buffer_size);

  // Specify the latency info to be passed to the browser side.
  current_sink_params_.set_latency_tag(AudioDeviceFactory::GetSourceLatencyType(
      GetLatencyHintSourceType(latency_hint_.Category())));
  SendLogMessage(
      base::StringPrintf("%s => (sink_params=[%s])", __func__,
                         current_sink_params_.AsHumanReadableString().c_str()));

  if (base::FeatureList::IsEnabled(media::kLiveCaptionWebAudio)) {
    auto* web_local_frame =
        WebLocalFrame::FromFrameToken(sink_descriptor_.Token());
    if (web_local_frame) {
      speech_recognition_client_ =
          web_local_frame->Client()->CreateSpeechRecognitionClient();
      if (speech_recognition_client_) {
        speech_recognition_client_->Reconfigure(current_sink_params_);
      }
    }
  }

  InitializeSink();

  if (status != media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK) {
    Stop();
  }
}

void RendererWebAudioDeviceImpl::InitializeSink() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_sink_initialized_ || !sink_) {
    return;
  }

  switch (sink_descriptor_.Type()) {
    case blink::WebAudioSinkDescriptor::kAudible:
      // Use a task runner instead of the render thread for fake Render() calls
      // since it has special connotations for Blink and garbage collection.
      // Timeout value chosen to be highly unlikely in the normal case.
      silent_sink_suspender_ = std::make_unique<media::SilentSinkSuspender>(
          this, base::Seconds(30), current_sink_params_, sink_,
          GetSilentSinkTaskRunner());
      silent_sink_suspender_->SetDetectSilence(is_detecting_silence_);
      sink_->Initialize(current_sink_params_, silent_sink_suspender_.get());
      break;
    case blink::WebAudioSinkDescriptor::kSilent:
      sink_->Initialize(current_sink_params_, this);
      break;
  }
  is_sink_initialized_ = true;
}

RendererWebAudioDeviceImpl::~RendererWebAudioDeviceImpl() {
  Stop();
}

void RendererWebAudioDeviceImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT1("webaudio", "RendererWebAudioDeviceImpl::Start", "sink_id",
               sink_descriptor_.SinkId().Utf8());
  SendLogMessage(base::StringPrintf("%s", __func__));

  if (!is_stopped_) {
    return;
  }

  if (!sink_) {
    CreateAudioRendererSink();
  }

  if (!is_sink_initialized_) {
    InitializeSink();
  }

  sink_->Start();
  sink_->Play();
  is_stopped_ = false;
}

void RendererWebAudioDeviceImpl::Pause() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT1("webaudio", "RendererWebAudioDeviceImpl::Pause", "sink_id",
               sink_descriptor_.SinkId().Utf8());
  SendLogMessage(base::StringPrintf("%s", __func__));
  if (sink_) {
    sink_->Pause();
  }
  if (silent_sink_suspender_) {
    silent_sink_suspender_->OnPaused();
  }
}

void RendererWebAudioDeviceImpl::Resume() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT1("webaudio", "RendererWebAudioDeviceImpl::Resume", "sink_id",
               sink_descriptor_.SinkId().Utf8());
  SendLogMessage(base::StringPrintf("%s", __func__));
  if (sink_) {
    sink_->Play();
  }
}

void RendererWebAudioDeviceImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT1("webaudio", "RendererWebAudioDeviceImpl::Stop", "sink_id",
               sink_descriptor_.SinkId().Utf8());
  SendLogMessage(base::StringPrintf("%s", __func__));
  // If active, pause the silent sink suspender before stopping the sink to
  // ensure no callbacks are executed during teardown.
  if (silent_sink_suspender_) {
    silent_sink_suspender_->OnPaused();
  }
  if (sink_) {
    sink_->Stop();
    sink_ = nullptr;
  }

  silent_sink_suspender_.reset();
  is_sink_initialized_ = false;
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
  is_detecting_silence_ = enable_silence_detection;

  if (silent_sink_suspender_) {
    silent_sink_suspender_->SetDetectSilence(enable_silence_detection);
  }
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
  // triggers the bound callback posted to the main thread.
  if (render_error_callback_) {
    render_error_callback_.Run();
  }
}

void RendererWebAudioDeviceImpl::NotifyRenderError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SendLogMessage(base::StringPrintf("%s", __func__));

  webaudio_callback_->OnRenderError();
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
  blink::WebRtcLogMessage(base::StrCat({"[WA]RWADI::", message}));
}

void RendererWebAudioDeviceImpl::CreateAudioRendererSink() {
  TRACE_EVENT2("webaudio",
               "RendererWebAudioDeviceImpl::CreateAudioRendererSink",
               "sink_type", static_cast<int>(sink_descriptor_.Type()),
               "sink_id", sink_descriptor_.SinkId().Utf8());
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!sink_);

  switch (sink_descriptor_.Type()) {
    case blink::WebAudioSinkDescriptor::kAudible:
      sink_ = AudioDeviceFactory::GetInstance()->NewAudioRendererSink(
          GetLatencyHintSourceType(latency_hint_.Category()),
          sink_descriptor_.Token(),
          media::AudioSinkParameters(base::UnguessableToken(),
                                     sink_descriptor_.SinkId().Utf8()));
      break;
    case blink::WebAudioSinkDescriptor::kSilent:
      sink_ = create_silent_sink_cb_.Run(GetSilentSinkTaskRunner());
      break;
  }
  is_sink_initialized_ = false;
}

media::OutputDeviceStatus
RendererWebAudioDeviceImpl::MaybeCreateSinkAndGetStatus() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!sink_) {
    CreateAudioRendererSink();
  }

  media::OutputDeviceInfo device_info = GetSinkOutputDeviceInfo();
  media::OutputDeviceStatus status = device_info.device_status();

  if (!is_sink_initialized_) {
    HandleDeviceStatus(std::move(device_info));
  } else if (status != media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK) {
    Stop();
  }

  TRACE_EVENT2("webaudio",
               "RendererWebAudioDeviceImpl::MaybeCreateSinkAndGetStatus",
               "sink_id", sink_descriptor_.SinkId().Utf8(),
               "status", static_cast<int>(status));

  return status;
}

}  // namespace content
