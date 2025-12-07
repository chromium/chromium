// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/audio/audio_source_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"
#include "media/base/sample_rates.h"
#include "media/media_buildflags.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/audio_service_audio_processor_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/media/base/media_channel.h"

using base::StringPrintf;

namespace blink {

namespace {

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("PLAS::" + message);
}

// Used as an identifier for ProcessedLocalAudioSource::From().
void* const kProcessedLocalAudioSourceIdentifier =
    const_cast<void**>(&kProcessedLocalAudioSourceIdentifier);

std::string EffectsToString(int effects) {
  return media::AudioParameters::EffectsMaskToString(effects);
}

std::string GetEnsureSourceIsStartedLogString(
    const blink::MediaStreamDevice& device) {
  return base::StringPrintf(
      "EnsureSourceIsStarted({channel_layout=%d}, "
      "{sample_rate=%d}, {buffer_size=%d}, {effects=[%s]})[session_id=%s]",
      device.input.channel_layout(), device.input.sample_rate(),
      device.input.frames_per_buffer(), EffectsToString(device.input.effects()),
      device.session_id().ToString().c_str());
}

void LogInputDeviceParametersToUma(
    const media::AudioParameters& input_device_params) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            input_device_params.channel_layout(),
                            media::CHANNEL_LAYOUT_MAX + 1);
  media::AudioSampleRate asr;
  if (media::ToAudioSampleRate(input_device_params.sample_rate(), &asr)) {
    UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputSampleRate", asr,
                              media::kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS_1M("WebRTC.AudioInputSampleRateUnexpected",
                            input_device_params.sample_rate());
  }
}

}  // namespace

ProcessedLocalAudioSource::ProcessedLocalAudioSource(
    LocalFrame& frame,
    const blink::MediaStreamDevice& device,
    bool disable_local_echo,
    const MediaStreamAudioProcessingLayout& processing_layout,
    ConstraintsRepeatingCallback started_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : blink::MediaStreamAudioSource(std::move(task_runner),
                                    true /* is_local_source */,
                                    disable_local_echo),
      consumer_frame_(&frame),
      dependency_factory_(
          PeerConnectionDependencyFactory::From(*frame.DomWindow())),
      processing_layout_(processing_layout),
      started_callback_(std::move(started_callback)),
      allow_invalid_render_frame_id_for_testing_(false) {
  DCHECK(frame.DomWindow());
  SetDevice(device);
  SendLogMessage(StringPrintf(
      "%s({audio_processing_properties=[%s]}, {APM=%s})[session_id=%s]",
      __func__, processing_layout.properties().ToString(),
      processing_layout_.NeedApmInAudioService() ? "remote" : "local",
      device.session_id().ToString()));
}

ProcessedLocalAudioSource::~ProcessedLocalAudioSource() {
  DVLOG(1) << "PLAS::~ProcessedLocalAudioSource()";
  EnsureSourceIsStopped();
}

// static
ProcessedLocalAudioSource* ProcessedLocalAudioSource::From(
    blink::MediaStreamAudioSource* source) {
  if (source &&
      source->GetClassIdentifier() == kProcessedLocalAudioSourceIdentifier)
    return static_cast<ProcessedLocalAudioSource*>(source);
  return nullptr;
}

void ProcessedLocalAudioSource::SendLogMessageWithSessionId(
    const std::string& message) const {
  SendLogMessage(message + " [session_id=" + device().session_id().ToString() +
                 "]");
}

std::optional<blink::AudioProcessingProperties>
ProcessedLocalAudioSource::GetAudioProcessingProperties() const {
  return processing_layout_.properties();
}

void* ProcessedLocalAudioSource::GetClassIdentifier() const {
  return kProcessedLocalAudioSourceIdentifier;
}

bool ProcessedLocalAudioSource::EnsureSourceIsStarted() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (source_)
    return true;

  // Sanity-check that the consuming RenderFrame still exists. This is required
  // to initialize the audio source.
  if (!allow_invalid_render_frame_id_for_testing_ && !consumer_frame_) {
    SendLogMessageWithSessionId(
        "EnsureSourceIsStarted() => (ERROR: "
        " render frame does not exist)");
    return false;
  }

  SendLogMessage(GetEnsureSourceIsStartedLogString(device()));

  if (processing_layout_.platform_effects() != device().input.effects()) {
    SendLogMessage(
        StringPrintf("%s() => (Modified system effect mask from [%s] to [%s])",
                     __func__, EffectsToString(device().input.effects()),
                     EffectsToString(processing_layout_.platform_effects())));

    blink::MediaStreamDevice modified_device(device());
    modified_device.input.set_effects(processing_layout_.platform_effects());
    SetDevice(modified_device);
  }
  // Create the audio processor.

  DCHECK(dependency_factory_);
  WebRtcAudioDeviceImpl* const rtc_audio_device =
      dependency_factory_->GetWebRtcAudioDevice();
  if (!rtc_audio_device) {
    SendLogMessageWithSessionId(
        "EnsureSourceIsStarted() => (ERROR: no WebRTC ADM instance)");
    return false;
  }

  if (processing_layout_.NoiseSuppressionInTandem()) {
    SendLogMessage(StringPrintf("%s() => (NS will run in tandem)", __func__));
  }
  if (processing_layout_.AutomaticGainControlInTandem()) {
    SendLogMessage(StringPrintf("%s() => (AGC will run in tandem)", __func__));
  }

  // Determine the audio format required of the AudioCapturerSource.
  const media::AudioParameters input_device_params = device().input;
  LogInputDeviceParametersToUma(input_device_params);
  auto maybe_audio_capture_params = media::AudioProcessor::ComputeInputFormat(
      input_device_params, processing_layout_.webrtc_processing_settings());

  if (!maybe_audio_capture_params) {
    SendLogMessage(base::StringPrintf(
        "EnsureSourceIsStarted() => (ERROR: "
        "input device format (%s) is not supported.",
        input_device_params.AsHumanReadableString().c_str()));
    return false;
  }
  media::AudioParameters audio_capture_params = *maybe_audio_capture_params;

  media::AudioSourceParameters source_config(device().session_id());

  if (processing_layout_.NeedApmInAudioService()) {
    // Since audio processing will be applied in the audio service, we request
    // audio here in the audio processing output format to avoid forced
    // resampling.
    audio_capture_params = media::AudioProcessor::GetDefaultOutputFormat(
        audio_capture_params, processing_layout_.webrtc_processing_settings());

    // Create a proxy to the audio processor in the audio service.
    audio_processor_proxy_ =
        new webrtc::RefCountedObject<AudioServiceAudioProcessorProxy>();

    // The output format of this ProcessedLocalAudioSource is the audio capture
    // format.
    SetFormat(audio_capture_params);

    // Add processing to the AudioCapturerSource configuration.
    source_config.processing = processing_layout_.webrtc_processing_settings();

  } else {
    // Create the MediaStreamAudioProcessor, bound to the WebRTC audio device
    // module.

    // This callback has to be valid until MediaStreamAudioProcessor is stopped,
    // which happens in EnsureSourceIsStopped().
    MediaStreamAudioProcessor::DeliverProcessedAudioCallback
        processing_callback =
            ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                &ProcessedLocalAudioSource::DeliverProcessedAudio,
                CrossThreadUnretained(this)));

    media_stream_audio_processor_ =
        new webrtc::RefCountedObject<MediaStreamAudioProcessor>(
            std::move(processing_callback),
            processing_layout_.webrtc_processing_settings(),
            audio_capture_params, rtc_audio_device);

    // The output format of this ProcessedLocalAudioSource is the audio
    // processor's output format.
    SetFormat(media_stream_audio_processor_->output_format());
  }

  SendLogMessageWithSessionId(base::StringPrintf(
      "EnsureSourceIsStarted() => (using APM in %s process: "
      "settings=[%s])",
      audio_processor_proxy_ ? "audio" : "renderer",
      processing_layout_.webrtc_processing_settings().ToString().c_str()));

  // Start the source.
  SendLogMessageWithSessionId(base::StringPrintf(
      "EnsureSourceIsStarted() => (WebRTC audio source starts: "
      "input_parameters=[%s], output_parameters=[%s])",
      audio_capture_params.AsHumanReadableString().c_str(),
      GetAudioParameters().AsHumanReadableString().c_str()));
  auto* web_frame =
      static_cast<WebLocalFrame*>(WebFrame::FromCoreFrame(consumer_frame_));
  scoped_refptr<media::AudioCapturerSource> new_source =
      Platform::Current()->NewAudioCapturerSource(web_frame, source_config);
  new_source->Initialize(audio_capture_params, this);
  // We need to set the AGC control before starting the stream.
#if BUILDFLAG(IS_CHROMEOS)
  new_source->SetAutomaticGainControl(true);
#else
  new_source->SetAutomaticGainControl(
      processing_layout_.webrtc_processing_settings().automatic_gain_control);
#endif
  source_ = std::move(new_source);
  source_->Start();

  // Register this source with the WebRtcAudioDeviceImpl.
  rtc_audio_device->AddAudioCapturer(this);

  return true;
}

void ProcessedLocalAudioSource::EnsureSourceIsStopped() {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  if (!source_)
    return;

  scoped_refptr<media::AudioCapturerSource> source_to_stop(std::move(source_));

  if (dependency_factory_) {
    dependency_factory_->GetWebRtcAudioDevice()->RemoveAudioCapturer(this);
  }

  source_to_stop->Stop();

  if (media_stream_audio_processor_) {
    // Stop the audio processor to avoid feeding render data into the processor.
    media_stream_audio_processor_->Stop();
  } else {
    // Stop the proxy, to detach from the processor controls.
    DCHECK(audio_processor_proxy_);
    audio_processor_proxy_->Stop();
  }

  DVLOG(1) << "Stopped WebRTC audio pipeline for consumption.";
}

scoped_refptr<webrtc::AudioProcessorInterface>
ProcessedLocalAudioSource::GetAudioProcessor() const {
  if (audio_processor_proxy_) {
    return static_cast<scoped_refptr<webrtc::AudioProcessorInterface>>(
        audio_processor_proxy_);
  }
  DCHECK(media_stream_audio_processor_);
  if (!media_stream_audio_processor_->has_webrtc_audio_processing())
    return nullptr;
  return static_cast<scoped_refptr<webrtc::AudioProcessorInterface>>(
      media_stream_audio_processor_);
}

void ProcessedLocalAudioSource::SetVolume(double volume) {
  DVLOG(1) << "ProcessedLocalAudioSource::SetVolume()";
  DCHECK_LE(volume, 1.0);
  if (source_)
    source_->SetVolume(volume);
}

void ProcessedLocalAudioSource::OnCaptureStarted() {
  SendLogMessageWithSessionId(base::StringPrintf("OnCaptureStarted()"));
  started_callback_.Run(this, mojom::blink::MediaStreamRequestResult::OK, "");
}

void ProcessedLocalAudioSource::Capture(
    const media::AudioBus* audio_bus,
    base::TimeTicks audio_capture_time,
    const media::AudioGlitchInfo& glitch_info,
    double volume) {
  TRACE_EVENT1("audio", "ProcessedLocalAudioSource::Capture", "capture-time",
               audio_capture_time);
  glitch_info_accumulator_.Add(glitch_info);
  // Maximum number of channels used by the sinks.
  int num_preferred_channels = NumPreferredChannels();
  if (media_stream_audio_processor_) {
    // Figure out if the pre-processed data has any energy or not. This
    // information will be passed to the level calculator to force it to report
    // energy in case the post-processed data is zeroed by the audio processing.
    force_report_nonzero_energy_ = !audio_bus->AreFramesZero();

    // Push the data to the processor for processing.
    // Passing audio to the audio processor is sufficient, the processor will
    // return it to DeliverProcessedAudio() via the registered callback.
    media_stream_audio_processor_->ProcessCapturedAudio(
        *audio_bus, audio_capture_time, num_preferred_channels, volume);
    return;
  }

  DCHECK(audio_processor_proxy_);
  audio_processor_proxy_->MaybeUpdateNumPreferredCaptureChannels(
      num_preferred_channels);

  // The audio is already processed in the audio service, just send it
  // along.
  force_report_nonzero_energy_ = false;
  DeliverProcessedAudio(*audio_bus, audio_capture_time,
                        /*new_volume=*/std::nullopt);
}

void ProcessedLocalAudioSource::OnCaptureError(
    media::AudioCapturerSource::ErrorCode code,
    const std::string& message) {
  SendLogMessageWithSessionId(
      base::StringPrintf("OnCaptureError({code=%d, message=%s})",
                         static_cast<int>(code), message.c_str()));
  StopSourceOnError(code, message);
}

void ProcessedLocalAudioSource::OnCaptureMuted(bool is_muted) {
  SendLogMessageWithSessionId(base::StringPrintf(
      "OnCaptureMuted({is_muted=%s})", base::ToString(is_muted).c_str()));
  SetMutedState(is_muted);
}

void ProcessedLocalAudioSource::OnCaptureProcessorCreated(
    media::AudioProcessorControls* controls) {
  SendLogMessageWithSessionId(
      base::StringPrintf("OnCaptureProcessorCreated()"));
  DCHECK_NE(!!media_stream_audio_processor_, !!audio_processor_proxy_);
  if (audio_processor_proxy_)
    audio_processor_proxy_->SetControls(controls);
}

void ProcessedLocalAudioSource::ChangeSourceImpl(
    const MediaStreamDevice& new_device) {
  DCHECK(GetTaskRunner()->BelongsToCurrentThread());

  // Source changes are not supported for microphone audio capture.
  CHECK_NE(new_device.type,
           mojom::blink::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  CHECK_NE(device().type, mojom::blink::MediaStreamType::DEVICE_AUDIO_CAPTURE);

  WebRtcLogMessage("ProcessedLocalAudioSource::ChangeSourceImpl(new_device = " +
                   new_device.id + ")");
  EnsureSourceIsStopped();
  SetDevice(new_device);
  EnsureSourceIsStarted();
}

void ProcessedLocalAudioSource::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  SendLogMessageWithSessionId(base::StringPrintf(
      "SetOutputDeviceForAec({device_id=%s})", output_device_id.c_str()));
  if (source_)
    source_->SetOutputDeviceForAec(output_device_id);
}

void ProcessedLocalAudioSource::DeliverProcessedAudio(
    const media::AudioBus& processed_audio,
    base::TimeTicks audio_capture_time,
    std::optional<double> new_volume) {
  TRACE_EVENT("audio", "ProcessedLocalAudioSource::DeliverProcessedAudio",
              "capture_time (ms)",
              (audio_capture_time - base::TimeTicks()).InMillisecondsF(),
              "capture_delay (ms)",
              (base::TimeTicks::Now() - audio_capture_time).InMillisecondsF());
  level_calculator_.Calculate(processed_audio, force_report_nonzero_energy_);
  DeliverDataToTracks(processed_audio, audio_capture_time,
                      glitch_info_accumulator_.GetAndReset());

  if (new_volume) {
    PostCrossThreadTask(
        *GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&ProcessedLocalAudioSource::SetVolume,
                            weak_factory_.GetWeakPtr(), *new_volume));
  }
}

}  // namespace blink
