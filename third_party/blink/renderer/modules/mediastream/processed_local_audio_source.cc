// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/audio/audio_source_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/sample_rates.h"
#include "media/webrtc/audio_processor_controls.h"
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
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/media/base/media_channel.h"

namespace blink {

using EchoCancellationType =
    blink::AudioProcessingProperties::EchoCancellationType;

namespace {

void SendLogMessage(const std::string& message) {
  blink::WebRtcLogMessage("PLAS::" + message);
}

// Used as an identifier for ProcessedLocalAudioSource::From().
void* const kProcessedLocalAudioSourceIdentifier =
    const_cast<void**>(&kProcessedLocalAudioSourceIdentifier);

std::string GetEnsureSourceIsStartedLogString(
    const blink::MediaStreamDevice& device) {
  return base::StringPrintf(
      "EnsureSourceIsStarted({session_id=%s}, {channel_layout=%d}, "
      "{sample_rate=%d}, {buffer_size=%d}, {effects=%d})",
      device.session_id().ToString().c_str(), device.input.channel_layout(),
      device.input.sample_rate(), device.input.frames_per_buffer(),
      device.input.effects());
}

std::string GetAudioProcesingPropertiesLogString(
    const blink::AudioProcessingProperties& properties) {
  auto aec_to_string =
      [](blink::AudioProcessingProperties::EchoCancellationType type) {
        using AEC = blink::AudioProcessingProperties::EchoCancellationType;
        switch (type) {
          case AEC::kEchoCancellationDisabled:
            return "disabled";
          case AEC::kEchoCancellationAec3:
            return "aec3";
          case AEC::kEchoCancellationSystem:
            return "system";
        }
      };
  auto bool_to_string = [](bool value) { return value ? "true" : "false"; };
  auto str = base::StringPrintf(
      "aec: %s, "
      "disable_hw_ns: %s, "
      "goog_audio_mirroring: %s, "
      "goog_auto_gain_control: %s, "
      "goog_experimental_echo_cancellation: %s, "
      "goog_noise_suppression: %s, "
      "goog_experimental_noise_suppression: %s, "
      "goog_highpass_filter: %s, "
      "goog_experimental_agc: %s, "
      "hybrid_agc: %s"
      "analog_agc_clipping_control: %s",
      aec_to_string(properties.echo_cancellation_type),
      bool_to_string(properties.disable_hw_noise_suppression),
      bool_to_string(properties.goog_audio_mirroring),
      bool_to_string(properties.goog_auto_gain_control),
      bool_to_string(properties.goog_experimental_echo_cancellation),
      bool_to_string(properties.goog_noise_suppression),
      bool_to_string(properties.goog_experimental_noise_suppression),
      bool_to_string(properties.goog_highpass_filter),
      bool_to_string(properties.goog_experimental_auto_gain_control),
      bool_to_string(
          base::FeatureList::IsEnabled(::features::kWebRtcHybridAgc)),
      bool_to_string(base::FeatureList::IsEnabled(
          ::features::kWebRtcAnalogAgcClippingControl)));
  return str;
}

// Returns whether system noise suppression is allowed to be used regardless of
// whether the noise suppression constraint is set, or whether a browser-based
// AEC is active. This is currently the default on at least MacOS but is not
// allowed for ChromeOS setups.
constexpr bool IsIndependentSystemNsAllowed() {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

int GetCaptureBufferSize(bool need_webrtc_processing,
                         const media::AudioParameters input_device_params) {
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMECAST)
  // TODO(henrika): Re-evaluate whether to use same logic as other platforms.
  // https://crbug.com/638081
  return 2 * input_device_params.sample_rate() / 100;
#else
  // If audio processing is turned on, require 10ms buffers.
  if (need_webrtc_processing)
    return input_device_params.sample_rate() / 100;

  // If audio processing is off and the native hardware buffer size was
  // provided, use it. It can be harmful, in terms of CPU/power consumption,
  // to use smaller buffer sizes than the native size
  // (https://crbug.com/362261).
  if (int hardware_buffer_size = input_device_params.frames_per_buffer())
    return hardware_buffer_size;

  // If the buffer size is missing from the MediaStreamDevice, provide 10ms as
  // a fall-back.
  return input_device_params.sample_rate() / 100;
#endif
}

// Will return nullopt if |input_device_params| are not supported.
absl::optional<media::AudioParameters> ComputeAudioCaptureParams(
    const media::AudioParameters& input_device_params,
    const media::AudioProcessingSettings& audio_processing_settings) {
  const media::ChannelLayout channel_layout =
      input_device_params.channel_layout();
  DVLOG(1) << "Audio input hardware channel layout: " << channel_layout;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout", channel_layout,
                            media::CHANNEL_LAYOUT_MAX + 1);

  // Verify that the reported input channel configuration is supported.
  if (channel_layout != media::CHANNEL_LAYOUT_MONO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO &&
      channel_layout != media::CHANNEL_LAYOUT_DISCRETE) {
    SendLogMessage(
        base::StringPrintf("EnsureSourceIsStarted() => (ERROR: "
                           "input channel layout (%d) is not supported.",
                           static_cast<int>(channel_layout)));
    return absl::nullopt;
  }

  DVLOG(1) << "Audio input hardware sample rate: "
           << input_device_params.sample_rate();
  media::AudioSampleRate asr;
  if (media::ToAudioSampleRate(input_device_params.sample_rate(), &asr)) {
    UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputSampleRate", asr,
                              media::kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS_1M("WebRTC.AudioInputSampleRateUnexpected",
                            input_device_params.sample_rate());
  }

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout,
      input_device_params.sample_rate(),
      GetCaptureBufferSize(
          audio_processing_settings.NeedWebrtcAudioProcessing(),
          input_device_params));
  params.set_effects(input_device_params.effects());
  if (channel_layout == media::CHANNEL_LAYOUT_DISCRETE) {
    DCHECK_LE(input_device_params.channels(), 2);
    params.set_channels_for_discrete(input_device_params.channels());
  }
  DVLOG(1) << params.AsHumanReadableString();
  CHECK(params.IsValid());
  return params;
}

}  // namespace

ProcessedLocalAudioSource::ProcessedLocalAudioSource(
    LocalFrame& frame,
    const blink::MediaStreamDevice& device,
    bool disable_local_echo,
    const blink::AudioProcessingProperties& audio_processing_properties,
    int num_requested_channels,
    ConstraintsOnceCallback started_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : blink::MediaStreamAudioSource(std::move(task_runner),
                                    true /* is_local_source */,
                                    disable_local_echo),
      consumer_frame_(&frame),
      dependency_factory_(
          PeerConnectionDependencyFactory::From(*frame.DomWindow())),
      audio_processing_properties_(audio_processing_properties),
      num_requested_channels_(num_requested_channels),
      started_callback_(std::move(started_callback)),
      allow_invalid_render_frame_id_for_testing_(false) {
  DCHECK(frame.DomWindow());
  SetDevice(device);
  SendLogMessage(
      base::StringPrintf("ProcessedLocalAudioSource({session_id=%s})",
                         device.session_id().ToString().c_str()));
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

absl::optional<blink::AudioProcessingProperties>
ProcessedLocalAudioSource::GetAudioProcessingProperties() const {
  return audio_processing_properties_;
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
  SendLogMessageWithSessionId(base::StringPrintf(
      "EnsureSourceIsStarted() => (audio_processing_properties=[%s])",
      GetAudioProcesingPropertiesLogString(audio_processing_properties_)
          .c_str()));

  blink::MediaStreamDevice modified_device(device());
  bool device_is_modified = false;

  // Disable system echo cancellation if specified by
  // |audio_processing_properties_|. Also disable any system noise suppression
  // and automatic gain control to avoid those causing issues for the echo
  // cancellation.
  if (audio_processing_properties_.echo_cancellation_type !=
          EchoCancellationType::kEchoCancellationSystem &&
      device().input.effects() & media::AudioParameters::ECHO_CANCELLER) {
    modified_device.input.set_effects(modified_device.input.effects() &
                                      ~media::AudioParameters::ECHO_CANCELLER);
    if (!IsIndependentSystemNsAllowed()) {
      modified_device.input.set_effects(
          modified_device.input.effects() &
          ~media::AudioParameters::NOISE_SUPPRESSION);
    }
    modified_device.input.set_effects(
        modified_device.input.effects() &
        ~media::AudioParameters::AUTOMATIC_GAIN_CONTROL);
    device_is_modified = true;
  } else if (audio_processing_properties_.echo_cancellation_type ==
                 EchoCancellationType::kEchoCancellationSystem &&
             (device().input.effects() &
              media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER)) {
    // Set the ECHO_CANCELLER effect, since that is what controls what's
    // actually being used. The EXPERIMENTAL_ flag only indicates availability.
    // TODO(grunell): AND with
    // ~media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER.
    modified_device.input.set_effects(modified_device.input.effects() |
                                      media::AudioParameters::ECHO_CANCELLER);
    device_is_modified = true;
  }

  // Optionally disable system noise suppression.
  if (device().input.effects() & media::AudioParameters::NOISE_SUPPRESSION) {
    // Disable noise suppression on the device if the properties explicitly
    // specify to do so.
    bool disable_system_noise_suppression =
        audio_processing_properties_.disable_hw_noise_suppression;

    if (!IsIndependentSystemNsAllowed()) {
      // Disable noise suppression on the device if browser-based echo
      // cancellation is active, since that otherwise breaks the AEC.
      const bool browser_based_aec_active =
          audio_processing_properties_.echo_cancellation_type ==
          AudioProcessingProperties::EchoCancellationType::
              kEchoCancellationAec3;
      disable_system_noise_suppression =
          disable_system_noise_suppression || browser_based_aec_active;

      // Disable noise suppression on the device if the constraints
      // dictate that.
      disable_system_noise_suppression =
          disable_system_noise_suppression ||
          !audio_processing_properties_.goog_noise_suppression;
    }

    if (disable_system_noise_suppression) {
      modified_device.input.set_effects(
          modified_device.input.effects() &
          ~media::AudioParameters::NOISE_SUPPRESSION);
      device_is_modified = true;
    }
  }

  // Optionally disable system automatic gain control.
  if (device().input.effects() &
      media::AudioParameters::AUTOMATIC_GAIN_CONTROL) {
    // Disable automatic gain control on the device if browser-based echo
    // cancellation is, since that otherwise breaks the AEC.
    const bool browser_based_aec_active =
        audio_processing_properties_.echo_cancellation_type ==
        AudioProcessingProperties::EchoCancellationType::kEchoCancellationAec3;
    bool disable_system_automatic_gain_control = browser_based_aec_active;

    // Disable automatic gain control on the device if the constraints dictates
    // that.
    disable_system_automatic_gain_control =
        disable_system_automatic_gain_control ||
        !audio_processing_properties_.goog_auto_gain_control;

    if (disable_system_automatic_gain_control) {
      modified_device.input.set_effects(
          modified_device.input.effects() &
          ~media::AudioParameters::AUTOMATIC_GAIN_CONTROL);
      device_is_modified = true;
    }
  }

  if (device_is_modified)
    SetDevice(modified_device);

  // Create the MediaStreamAudioProcessor, bound to the WebRTC audio device
  // module.
  DCHECK(dependency_factory_);
  WebRtcAudioDeviceImpl* const rtc_audio_device =
      dependency_factory_->GetWebRtcAudioDevice();
  if (!rtc_audio_device) {
    SendLogMessageWithSessionId(
        "EnsureSourceIsStarted() => (ERROR: no WebRTC ADM instance)");
    return false;
  }

  // If system level echo cancellation is active, flag any other active system
  // level effects to the media stream audio processor.
  if (audio_processing_properties_.echo_cancellation_type ==
      AudioProcessingProperties::EchoCancellationType::
          kEchoCancellationSystem) {
    if (!IsIndependentSystemNsAllowed()) {
      if (audio_processing_properties_.goog_noise_suppression) {
        audio_processing_properties_.system_noise_suppression_activated =
            device().input.effects() &
            media::AudioParameters::NOISE_SUPPRESSION;
      }
    }

    if (audio_processing_properties_.goog_auto_gain_control) {
      audio_processing_properties_.system_gain_control_activated =
          device().input.effects() &
          media::AudioParameters::AUTOMATIC_GAIN_CONTROL;
    }
  }

  // No more modifications of |audio_processing_properties_| after this line.
  media::AudioProcessingSettings audio_processing_settings(
      audio_processing_properties_.ToAudioProcessingSettings(
          num_requested_channels_ > 1));

  // Determine the audio format required of the AudioCapturerSource. Then,
  // pass that to the |media_stream_audio_processor_| and set the output format
  // of this ProcessedLocalAudioSource to the processor's output format.
  auto maybe_audio_capture_params =
      ComputeAudioCaptureParams(device().input, audio_processing_settings);
  if (!maybe_audio_capture_params)  // Unsupported configuration.
    return false;

  media::AudioParameters& audio_capture_params = *maybe_audio_capture_params;

  blink::WebRtcLogMessage("Using APM in renderer process.");

  // This callback has to be valid until MediaStreamAudioProcessor is stopped,
  // which happens in EnsureSourceIsStopped().
  MediaStreamAudioProcessor::DeliverProcessedAudioCallback processing_callback =
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &ProcessedLocalAudioSource::DeliverProcessedAudio,
          CrossThreadUnretained(this)));

  media_stream_audio_processor_ =
      new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          std::move(processing_callback), audio_processing_settings,
          audio_capture_params, rtc_audio_device);

  SetFormat(media_stream_audio_processor_->OutputFormat());

  // Start the source.
  SendLogMessageWithSessionId(base::StringPrintf(
      "EnsureSourceIsStarted() => (WebRTC audio source starts: "
      "input_parameters=[%s], output_parameters=[%s])",
      audio_capture_params.AsHumanReadableString().c_str(),
      GetAudioParameters().AsHumanReadableString().c_str()));
  auto* web_frame =
      static_cast<WebLocalFrame*>(WebFrame::FromCoreFrame(consumer_frame_));
  media::AudioSourceParameters source_params(device().session_id());
  scoped_refptr<media::AudioCapturerSource> new_source =
      Platform::Current()->NewAudioCapturerSource(web_frame, source_params);
  new_source->Initialize(audio_capture_params, this);
  // We need to set the AGC control before starting the stream.
  new_source->SetAutomaticGainControl(true);
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

  // Stop the audio processor to avoid feeding render data into the processor.
  if (media_stream_audio_processor_)
    media_stream_audio_processor_->Stop();

  DVLOG(1) << "Stopped WebRTC audio pipeline for consumption.";
}

scoped_refptr<webrtc::AudioProcessorInterface>
ProcessedLocalAudioSource::GetAudioProcessor() const {
  DCHECK(media_stream_audio_processor_);
  return static_cast<scoped_refptr<webrtc::AudioProcessorInterface>>(
      media_stream_audio_processor_);
}

bool ProcessedLocalAudioSource::HasWebRtcAudioProcessing() const {
  return media_stream_audio_processor_ &&
         media_stream_audio_processor_->has_webrtc_audio_processing();
}

void ProcessedLocalAudioSource::SetOutputWillBeMuted(bool muted) {
  if (base::FeatureList::IsEnabled(
          features::kMinimizeAudioProcessingForUnusedOutput) &&
      HasWebRtcAudioProcessing()) {
    media_stream_audio_processor_->SetOutputWillBeMuted(muted);
  }
}

void ProcessedLocalAudioSource::SetVolume(double volume) {
  DVLOG(1) << "ProcessedLocalAudioSource::SetVolume()";
  DCHECK_LE(volume, 1.0);
  if (source_)
    source_->SetVolume(volume);
}

void ProcessedLocalAudioSource::OnCaptureStarted() {
  SendLogMessageWithSessionId(base::StringPrintf("OnCaptureStarted()"));
  std::move(started_callback_)
      .Run(this, blink::mojom::MediaStreamRequestResult::OK, "");
}

void ProcessedLocalAudioSource::Capture(const media::AudioBus* audio_bus,
                                        base::TimeTicks audio_capture_time,
                                        double volume,
                                        bool key_pressed) {
  TRACE_EVENT1("audio", "ProcessedLocalAudioSource::Capture", "capture-time",
               audio_capture_time);
  if (media_stream_audio_processor_) {
    // Figure out if the pre-processed data has any energy or not. This
    // information will be passed to the level calculator to force it to report
    // energy in case the post-processed data is zeroed by the audio processing.
    force_report_nonzero_energy_ = !audio_bus->AreFramesZero();

    // Push the data to the processor for processing.
    // Maximum number of channels used by the sinks.
    const int num_preferred_channels = NumPreferredChannels();

    // Passing audio to the audio processor is sufficient, the processor will
    // return it to DeliverProcessedAudio() via the registered callback.
    media_stream_audio_processor_->ProcessCapturedAudio(
        *audio_bus, audio_capture_time, num_preferred_channels, volume,
        key_pressed);
  } else {
    // The audio is already processed in the audio service, just send it
    // along.
    force_report_nonzero_energy_ = false;
    DeliverProcessedAudio(*audio_bus, audio_capture_time,
                          /*new_volume=*/absl::nullopt);
  }
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
      "OnCaptureMuted({is_muted=%s})", is_muted ? "true" : "false"));
  SetMutedState(is_muted);
}

void ProcessedLocalAudioSource::OnCaptureProcessorCreated(
    media::AudioProcessorControls* controls) {
  SendLogMessageWithSessionId(
      base::StringPrintf("OnCaptureProcessorCreated()"));
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
    absl::optional<double> new_volume) {
  TRACE_EVENT1("audio", "ProcessedLocalAudioSource::DeliverProcessedAudio",
               "capture-time", audio_capture_time);
  level_calculator_.Calculate(processed_audio, force_report_nonzero_energy_);
  DeliverDataToTracks(processed_audio, audio_capture_time);

  if (new_volume) {
    PostCrossThreadTask(
        *GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&ProcessedLocalAudioSource::SetVolume,
                            weak_factory_.GetWeakPtr(), *new_volume));
  }
}

}  // namespace blink
