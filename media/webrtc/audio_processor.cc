// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/audio_processor.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "media/webrtc/webrtc_switches.h"
#include "third_party/webrtc/api/audio/echo_canceller3_factory.h"
#include "third_party/webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"

namespace media {

namespace {

constexpr int kBuffersPerSecond = 100;  // 10 ms per buffer.

webrtc::AudioProcessing::ChannelLayout MediaLayoutToWebRtcLayout(
    ChannelLayout media_layout) {
  switch (media_layout) {
    case CHANNEL_LAYOUT_MONO:
      return webrtc::AudioProcessing::kMono;
    case CHANNEL_LAYOUT_STEREO:
    case CHANNEL_LAYOUT_DISCRETE:
      // TODO(https://crbug.com/868026): currently mapping all discrete channel
      // layouts to two channels assuming that any required channel remix takes
      // place in the native audio layer.
      return webrtc::AudioProcessing::kStereo;
    case CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC:
      return webrtc::AudioProcessing::kStereoAndKeyboard;
    default:
      NOTREACHED() << "Layout not supported: " << media_layout;
      return webrtc::AudioProcessing::kMono;
  }
}

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
  if (audio_processing_)
    audio_processing_->UpdateHistogramsOnCallEnd();
  // EchoInformation does this by itself on destruction, but since the stats are
  // reset, they won't get doubly reported.
  echo_information_.ReportAndResetAecDivergentFilterStats();
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

  constexpr int kMaxChannels = 2;
  DCHECK_GE(parameters.channels(), 1);
  DCHECK_LE(parameters.channels(), kMaxChannels);
  const float* channel_ptrs[kMaxChannels];
  channel_ptrs[0] = audio.channel(0);
  webrtc::AudioProcessing::ChannelLayout webrtc_layout =
      webrtc::AudioProcessing::ChannelLayout::kMono;
  if (parameters.channels() == 2) {
    channel_ptrs[1] = audio.channel(1);
    webrtc_layout = webrtc::AudioProcessing::ChannelLayout::kStereo;
  }

  const int apm_error = audio_processing_->AnalyzeReverseStream(
      channel_ptrs, parameters.frames_per_buffer(), parameters.sample_rate(),
      webrtc_layout);

  DCHECK_EQ(apm_error, webrtc::AudioProcessing::kNoError);
}

void AudioProcessor::UpdateInternalStats() {
  if (audio_processing_)
    echo_information_.UpdateAecStats(
        audio_processing_->GetStatistics(has_reverse_stream_));
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
    base::PostTaskWithTraits(
        FROM_HERE, {base::TaskPriority::LOWEST, base::MayBlock()},
        base::BindOnce([](base::File) {}, std::move(file)));
    return;
  }

  // Here tasks will be posted on the |worker_queue_|. It must be kept alive
  // until StopEchoCancellationDump is called or the webrtc::AudioProcessing
  // instance is destroyed.

  DCHECK(file.IsValid());

  base::PlatformFile stream = file.TakePlatformFile();
  if (!worker_queue_) {
    worker_queue_ = std::make_unique<rtc::TaskQueue>(
        "aecdump-worker-queue", rtc::TaskQueue::Priority::LOW);
  }
  auto aec_dump = webrtc::AecDumpFactory::Create(
      stream, -1 /* max_log_size_bytes */, worker_queue_.get());
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
  if (settings_.echo_cancellation != EchoCancellationType::kAec2 &&
      settings_.echo_cancellation != EchoCancellationType::kAec3 &&
      settings_.noise_suppression == NoiseSuppressionType::kDisabled &&
      settings_.automatic_gain_control == AutomaticGainControlType::kDisabled &&
      !settings_.high_pass_filter && !settings_.typing_detection) {
    return;
  }

  webrtc::Config ap_config;  // Note: ap_config.Set(x) transfers ownership of x.
  webrtc::AudioProcessingBuilder ap_builder;

  // AEC setup part 1.

  // AEC2 options. Doesn't do anything if AEC2 isn't used.
  ap_config.Set<webrtc::RefinedAdaptiveFilter>(
      new webrtc::RefinedAdaptiveFilter(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAecRefinedAdaptiveFilter)));
  ap_config.Set<webrtc::ExtendedFilter>(new webrtc::ExtendedFilter(true));
  ap_config.Set<webrtc::DelayAgnostic>(new webrtc::DelayAgnostic(true));

  // Echo cancellation is configured both before and after AudioProcessing
  // construction, but before Initialize.
  if (settings_.echo_cancellation == EchoCancellationType::kAec3) {
    webrtc::EchoCanceller3Config aec3_config;
    aec3_config.ep_strength.bounded_erl =
        base::FeatureList::IsEnabled(features::kWebRtcAecBoundedErlSetup);
    aec3_config.echo_removal_control.has_clock_drift =
        base::FeatureList::IsEnabled(features::kWebRtcAecClockDriftSetup);
    aec3_config.echo_audibility.use_stationary_properties =
        base::FeatureList::IsEnabled(features::kWebRtcAecNoiseTransparency);

    ap_builder.SetEchoControlFactory(
        std::make_unique<webrtc::EchoCanceller3Factory>(aec3_config));
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
  }

  // Noise suppression setup part 1.
  ap_config.Set<webrtc::ExperimentalNs>(new webrtc::ExperimentalNs(
      settings_.noise_suppression == NoiseSuppressionType::kExperimental));

  // Audio processing module construction.
  audio_processing_ = base::WrapUnique(ap_builder.Create(ap_config));

  // Noise suppression setup part 2.
  if (settings_.noise_suppression != NoiseSuppressionType::kDisabled) {
    int err = audio_processing_->noise_suppression()->set_level(
        webrtc::NoiseSuppression::kHigh);
    err |= audio_processing_->noise_suppression()->Enable(true);
    DCHECK_EQ(err, 0);
  }

  // Typing detection setup.
  if (settings_.typing_detection) {
    typing_detector_ = std::make_unique<webrtc::TypingDetection>();
    int err = audio_processing_->voice_detection()->Enable(true);
    err |= audio_processing_->voice_detection()->set_likelihood(
        webrtc::VoiceDetection::kVeryLowLikelihood);
    DCHECK_EQ(err, 0);

    // Configure the update period to 1s (100 * 10ms) in the typing detector.
    typing_detector_->SetParameters(0, 0, 0, 0, 0, 100);
  }

  // AGC setup part 2.
  if (settings_.automatic_gain_control != AutomaticGainControlType::kDisabled) {
    int err = audio_processing_->gain_control()->set_mode(
        webrtc::GainControl::kAdaptiveAnalog);
    err |= audio_processing_->gain_control()->Enable(true);
    DCHECK_EQ(err, 0);
  }
  if (settings_.automatic_gain_control == AutomaticGainControlType::kDefault) {
    int err = audio_processing_->gain_control()->set_mode(
        webrtc::GainControl::kAdaptiveAnalog);
    err |= audio_processing_->gain_control()->Enable(true);
    DCHECK_EQ(err, 0);
  }

  webrtc::AudioProcessing::Config apm_config = audio_processing_->GetConfig();

  // AEC setup part 2.
  apm_config.echo_canceller.enabled =
      settings_.echo_cancellation == EchoCancellationType::kAec2 ||
      settings_.echo_cancellation == EchoCancellationType::kAec3;
  apm_config.echo_canceller.mobile_mode = false;

  // High-pass filter setup.
  apm_config.high_pass_filter.enabled = settings_.high_pass_filter;

  // AGC setup part 3.
  if (settings_.automatic_gain_control ==
          AutomaticGainControlType::kExperimental ||
      settings_.automatic_gain_control ==
          AutomaticGainControlType::kHybridExperimental) {
    apm_config.gain_controller2.enabled =
        settings_.automatic_gain_control ==
        AutomaticGainControlType::kHybridExperimental;
    apm_config.gain_controller2.fixed_gain_db = 0.f;
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
  int err =
      audio_processing_->gain_control()->set_stream_analog_level(webrtc_volume);
  DCHECK_EQ(err, 0) << "set_stream_analog_level() error: " << err;
}

void AudioProcessor::FeedDataToAPM(const AudioBus& source) {
  std::vector<const float*> input_ptrs(source.channels());
  for (int i = 0; i < source.channels(); ++i) {
    input_ptrs[i] = source.channel(i);
  }

  const auto layout =
      MediaLayoutToWebRtcLayout(audio_parameters_.channel_layout());

  const int sample_rate = audio_parameters_.sample_rate();
  int err = audio_processing_->ProcessStream(
      input_ptrs.data(), audio_parameters_.frames_per_buffer(), sample_rate,
      layout, sample_rate, layout, output_ptrs_.data());
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;
}

void AudioProcessor::UpdateTypingDetected(bool key_pressed) {
  if (typing_detector_) {
    webrtc::VoiceDetection* vad = audio_processing_->voice_detection();
    DCHECK(vad->is_enabled());
    typing_detected_ =
        typing_detector_->Process(key_pressed, vad->stream_has_voice());
  }
}

base::Optional<double> AudioProcessor::GetNewVolumeFromAGC(double volume) {
  constexpr double kWebRtcMaxVolume = 255;
  const int webrtc_volume = volume * kWebRtcMaxVolume;
  const int new_webrtc_volume =
      audio_processing_->gain_control()->stream_analog_level();

  return new_webrtc_volume == webrtc_volume
             ? base::nullopt
             : base::Optional<double>(static_cast<double>(new_webrtc_volume) /
                                      kWebRtcMaxVolume);
}

}  // namespace media
