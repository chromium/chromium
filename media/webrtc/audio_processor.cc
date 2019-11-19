// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/audio_processor.h"

#include <array>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "media/base/limits.h"
#include "media/webrtc/helpers.h"
#include "media/webrtc/webrtc_switches.h"
#include "third_party/webrtc/api/audio/echo_canceller3_factory.h"
#include "third_party/webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace media {

namespace {

constexpr int kBuffersPerSecond = 100;  // 10 ms per buffer.

}  // namespace

AudioProcessor::ProcessingResult::ProcessingResult(
    const AudioBus& audio,
    base::Optional<double> new_volume)
    : audio(audio), new_volume(new_volume) {}
AudioProcessor::ProcessingResult::ProcessingResult(const ProcessingResult& b) =
    default;
AudioProcessor::ProcessingResult::~ProcessingResult() = default;

AudioProcessor::AudioProcessor(const AudioParameters& audio_parameters,
                               const AudioProcessingSettings& settings)
    : audio_parameters_(audio_parameters),
      settings_(settings),
      output_bus_(AudioBus::Create(audio_parameters_)),
      audio_delay_stats_reporter_(kBuffersPerSecond) {
  DCHECK(audio_parameters.IsValid());
  DCHECK_EQ(audio_parameters_.GetBufferDuration(),
            base::TimeDelta::FromMilliseconds(10));
  InitializeAPM();
  output_ptrs_.reserve(audio_parameters_.channels());
  for (int i = 0; i < audio_parameters_.channels(); ++i) {
    output_ptrs_.push_back(output_bus_->channel(i));
  }
}

AudioProcessor::~AudioProcessor() {
  StopEchoCancellationDump();
}

// Process the audio from source and return a pointer to the processed data.
AudioProcessor::ProcessingResult AudioProcessor::ProcessCapture(
    const AudioBus& source,
    base::TimeTicks capture_time,
    double volume,
    bool key_pressed) {
  base::Optional<double> new_volume;

  if (audio_processing_) {
    UpdateDelayEstimate(capture_time);
    UpdateAnalogLevel(volume);
    audio_processing_->set_stream_key_pressed(key_pressed);

    // Writes to |output_bus_|.
    FeedDataToAPM(source);

    UpdateTypingDetected(key_pressed);
    new_volume = GetNewVolumeFromAGC(volume);
  } else {
    source.CopyTo(output_bus_.get());
  }

  if (settings_.stereo_mirroring &&
      audio_parameters_.channel_layout() == CHANNEL_LAYOUT_STEREO) {
    output_bus_->SwapChannels(0, 1);
  }

  return {*output_bus_, new_volume};
}

void AudioProcessor::AnalyzePlayout(const AudioBus& audio,
                                    const AudioParameters& parameters,
                                    base::TimeTicks playout_time) {
  if (!audio_processing_)
    return;

  render_delay_ = playout_time - base::TimeTicks::Now();

  DCHECK_GE(parameters.channels(), 1);
  DCHECK_LE(parameters.channels(), audio.channels());
  DCHECK_LE(parameters.channels(), media::limits::kMaxChannels);
  webrtc::StreamConfig input_stream_config = CreateStreamConfig(parameters);
  // If the input audio appears to contain upmixed mono audio, then APM is only
  // given the left channel. This reduces computational complexity and improves
  // convergence of audio processing algorithms.
  // TODO(crbug.com/1023337): Ensure correct channel count in input audio bus.
  assume_upmixed_mono_playout_ =
      assume_upmixed_mono_playout_ && LeftAndRightChannelsAreSymmetric(audio);
  if (assume_upmixed_mono_playout_) {
    input_stream_config.set_num_channels(1);
  }

  std::array<const float*, media::limits::kMaxChannels> input_ptrs;
  for (int i = 0; i < static_cast<int>(input_stream_config.num_channels());
       ++i) {
    input_ptrs[i] = audio.channel(i);
  }

  const int apm_error = audio_processing_->AnalyzeReverseStream(
      input_ptrs.data(), input_stream_config);

  DCHECK_EQ(apm_error, webrtc::AudioProcessing::kNoError);
}

void AudioProcessor::GetStats(GetStatsCB callback) {
  webrtc::AudioProcessorInterface::AudioProcessorStatistics out = {};
  if (audio_processing_) {
    out.typing_noise_detected = typing_detected_;
    out.apm_statistics = audio_processing_->GetStatistics(has_reverse_stream_);
  }
  std::move(callback).Run(out);
}

void AudioProcessor::StartEchoCancellationDump(base::File file) {
  if (!audio_processing_) {
    // The destructor of File is blocking. Post it to a task runner to avoid
    // blocking the main thread.
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::TaskPriority::LOWEST, base::MayBlock()},
        base::BindOnce([](base::File) {}, std::move(file)));
    return;
  }

  // Here tasks will be posted on the |worker_queue_|. It must be kept alive
  // until StopEchoCancellationDump is called or the webrtc::AudioProcessing
  // instance is destroyed.

  DCHECK(file.IsValid());

  if (!worker_queue_) {
    worker_queue_ = std::make_unique<rtc::TaskQueue>(
        CreateWebRtcTaskQueue(rtc::TaskQueue::Priority::LOW));
  }
  auto aec_dump = webrtc::AecDumpFactory::Create(
      FileToFILE(std::move(file), "wb"), -1 /* max_log_size_bytes */,
      worker_queue_.get());
  if (!aec_dump) {
    // AecDumpFactory::Create takes ownership of stream even if it fails, so we
    // don't need to close it.
    LOG(ERROR) << "Failed to start AEC debug recording";
    return;
  }
  audio_processing_->AttachAecDump(std::move(aec_dump));
}

void AudioProcessor::StopEchoCancellationDump() {
  if (audio_processing_)
    audio_processing_->DetachAecDump();
  // Note that deleting an rtc::TaskQueue has to be done from the
  // thread that created it.
  worker_queue_.reset();
}

void AudioProcessor::InitializeAPM() {
  // Most features must have some configuration applied before constructing the
  // APM, and some after; the initialization is divided into "part 1" and "part
  // 2" in those cases.

  // If we use nothing but, possibly, audio mirroring, don't initialize the APM.
  if (settings_.echo_cancellation != EchoCancellationType::kAec3 &&
      settings_.noise_suppression == NoiseSuppressionType::kDisabled &&
      settings_.automatic_gain_control == AutomaticGainControlType::kDisabled &&
      !settings_.high_pass_filter && !settings_.typing_detection) {
    return;
  }

  webrtc::Config ap_config;  // Note: ap_config.Set(x) transfers ownership of x.
  webrtc::AudioProcessingBuilder ap_builder;

  // AEC setup part 1.

  // Echo cancellation is configured both before and after AudioProcessing
  // construction, but before Initialize.
  if (settings_.echo_cancellation == EchoCancellationType::kAec3) {
    ap_builder.SetEchoControlFactory(
        std::make_unique<webrtc::EchoCanceller3Factory>());
  }

  // AGC setup part 1.
  if (settings_.automatic_gain_control ==
          AutomaticGainControlType::kExperimental ||
      settings_.automatic_gain_control ==
          AutomaticGainControlType::kHybridExperimental) {
    std::string min_volume_str(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAgcStartupMinVolume));
    int startup_min_volume;
    // |startup_min_volume| set to 0 in case of failure/empty string, which is
    // the default.
    base::StringToInt(min_volume_str, &startup_min_volume);
    auto* experimental_agc =
        new webrtc::ExperimentalAgc(true, startup_min_volume);
    experimental_agc->digital_adaptive_disabled =
        settings_.automatic_gain_control ==
        AutomaticGainControlType::kHybridExperimental;
    ap_config.Set<webrtc::ExperimentalAgc>(experimental_agc);
  } else {
    ap_config.Set<webrtc::ExperimentalAgc>(new webrtc::ExperimentalAgc(false));
  }

  // Noise suppression setup part 1.
  ap_config.Set<webrtc::ExperimentalNs>(new webrtc::ExperimentalNs(
      settings_.noise_suppression == NoiseSuppressionType::kExperimental));

  // Audio processing module construction.
  audio_processing_ = base::WrapUnique(ap_builder.Create(ap_config));

  webrtc::AudioProcessing::Config apm_config = audio_processing_->GetConfig();

  // APM audio pipeline setup.
  apm_config.pipeline.experimental_multi_channel =
      base::FeatureList::IsEnabled(features::kWebRtcEnableMultiChannelApm);

  // Typing detection setup.
  if (settings_.typing_detection) {
    typing_detector_ = std::make_unique<webrtc::TypingDetection>();
    // Configure the update period to 1s (100 * 10ms) in the typing detector.
    typing_detector_->SetParameters(0, 0, 0, 0, 0, 100);

    apm_config.voice_detection.enabled = true;
  }

  // AEC setup part 2.
  apm_config.echo_canceller.enabled =
      settings_.echo_cancellation == EchoCancellationType::kAec3;
  apm_config.echo_canceller.mobile_mode = false;

  // High-pass filter setup.
  apm_config.high_pass_filter.enabled = settings_.high_pass_filter;

  // Noise suppression setup part 2.
  apm_config.noise_suppression.enabled =
      settings_.noise_suppression != NoiseSuppressionType::kDisabled;
  apm_config.noise_suppression.level =
      webrtc::AudioProcessing::Config::NoiseSuppression::kHigh;

  // AGC setup part 2.
  apm_config.gain_controller1.enabled =
      settings_.automatic_gain_control != AutomaticGainControlType::kDisabled;
  apm_config.gain_controller1.mode =
      webrtc::AudioProcessing::Config::GainController1::kAdaptiveAnalog;
  if (settings_.automatic_gain_control ==
          AutomaticGainControlType::kExperimental ||
      settings_.automatic_gain_control ==
          AutomaticGainControlType::kHybridExperimental) {
    apm_config.gain_controller2.enabled =
        settings_.automatic_gain_control ==
        AutomaticGainControlType::kHybridExperimental;
    apm_config.gain_controller2.fixed_digital.gain_db = 0.f;
    apm_config.gain_controller2.adaptive_digital.enabled = true;
    const bool use_peaks_not_rms = base::GetFieldTrialParamByFeatureAsBool(
        features::kWebRtcHybridAgc, "use_peaks_not_rms", false);
    using Shortcut =
        webrtc::AudioProcessing::Config::GainController2::LevelEstimator;
    apm_config.gain_controller2.adaptive_digital.level_estimator =
        use_peaks_not_rms ? Shortcut::kPeak : Shortcut::kRms;

    const int saturation_margin = base::GetFieldTrialParamByFeatureAsInt(
        features::kWebRtcHybridAgc, "saturation_margin", -1);
    if (saturation_margin != -1) {
      apm_config.gain_controller2.adaptive_digital.extra_saturation_margin_db =
          saturation_margin;
    }
  }
  audio_processing_->ApplyConfig(apm_config);
}

void AudioProcessor::UpdateDelayEstimate(base::TimeTicks capture_time) {
  const base::TimeDelta capture_delay = base::TimeTicks::Now() - capture_time;
  // Note: this delay calculation doesn't make sense, but it's how it's done
  // right now. Instead, the APM should probably attach the playout time to each
  // reference buffer it gets and store that internally?
  const base::TimeDelta render_delay = render_delay_.load();

  audio_delay_stats_reporter_.ReportDelay(capture_delay, render_delay);

  const base::TimeDelta total_delay = capture_delay + render_delay;
  audio_processing_->set_stream_delay_ms(total_delay.InMilliseconds());
  if (total_delay > base::TimeDelta::FromMilliseconds(300)) {
    DLOG(WARNING) << "Large audio delay, capture delay: "
                  << capture_delay.InMilliseconds() << "ms; render delay: "
                  << render_delay_.load().InMilliseconds() << "ms";
  }
}

void AudioProcessor::UpdateAnalogLevel(double volume) {
  DCHECK_LE(volume, 1.0);
  constexpr double kWebRtcMaxVolume = 255;
  const int webrtc_volume = volume * kWebRtcMaxVolume;
  audio_processing_->set_stream_analog_level(webrtc_volume);
}

void AudioProcessor::FeedDataToAPM(const AudioBus& source) {
  DCHECK_LE(source.channels(), media::limits::kMaxChannels);
  std::array<const float*, media::limits::kMaxChannels> input_ptrs;
  for (int i = 0; i < source.channels(); ++i) {
    input_ptrs[i] = source.channel(i);
  }

  const webrtc::StreamConfig config = CreateStreamConfig(audio_parameters_);
  int err = audio_processing_->ProcessStream(input_ptrs.data(), config, config,
                                             output_ptrs_.data());
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;
}

void AudioProcessor::UpdateTypingDetected(bool key_pressed) {
  if (typing_detector_) {
    // Ignore remote tracks to avoid unnecessary stats computation.
    auto voice_detected =
        audio_processing_->GetStatistics(false /* has_remote_tracks */)
            .voice_detected;
    DCHECK(voice_detected.has_value());
    typing_detected_ = typing_detector_->Process(key_pressed, *voice_detected);
  }
}

base::Optional<double> AudioProcessor::GetNewVolumeFromAGC(double volume) {
  constexpr double kWebRtcMaxVolume = 255;
  const int webrtc_volume = volume * kWebRtcMaxVolume;
  const int new_webrtc_volume =
      audio_processing_->recommended_stream_analog_level();

  return new_webrtc_volume == webrtc_volume
             ? base::nullopt
             : base::Optional<double>(static_cast<double>(new_webrtc_volume) /
                                      kWebRtcMaxVolume);
}

}  // namespace media
