// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/webrtc/helpers.h"
#include "media/webrtc/webrtc_switches.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/aec_dump_agent_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/audio/echo_canceller3_config.h"
#include "third_party/webrtc/api/audio/echo_canceller3_config_json.h"
#include "third_party/webrtc/api/audio/echo_canceller3_factory.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing_statistics.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace WTF {

template <typename T>
struct CrossThreadCopier<rtc::scoped_refptr<T>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = rtc::scoped_refptr<T>;
  static Type Copy(Type pointer) { return pointer; }
};

}  // namespace WTF

namespace blink {

using EchoCancellationType =
    blink::AudioProcessingProperties::EchoCancellationType;

namespace {

using webrtc::AudioProcessing;

bool UseMultiChannelCaptureProcessing() {
  return base::FeatureList::IsEnabled(
      features::kWebRtcEnableCaptureMultiChannelApm);
}

bool Allow48kHzApmProcessing() {
  return base::FeatureList::IsEnabled(
      features::kWebRtcAllow48kHzProcessingOnArm);
}

constexpr int kAudioProcessingNumberOfChannels = 1;
constexpr int kBuffersPerSecond = 100;  // 10 ms per buffer.

}  // namespace

// Wraps AudioBus to provide access to the array of channel pointers, since this
// is the type webrtc::AudioProcessing deals in. The array is refreshed on every
// channel_ptrs() call, and will be valid until the underlying AudioBus pointers
// are changed, e.g. through calls to SetChannelData() or SwapChannels().
//
// All methods are called on one of the capture or render audio threads
// exclusively.
class MediaStreamAudioBus {
 public:
  MediaStreamAudioBus(int channels, int frames)
      : bus_(media::AudioBus::Create(channels, frames)),
        channel_ptrs_(new float*[channels]) {
    // May be created in the main render thread and used in the audio threads.
    DETACH_FROM_THREAD(thread_checker_);
  }

  void ReattachThreadChecker() {
    DETACH_FROM_THREAD(thread_checker_);
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  media::AudioBus* bus() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return bus_.get();
  }

  float* const* channel_ptrs() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    for (int i = 0; i < bus_->channels(); ++i) {
      channel_ptrs_[i] = bus_->channel(i);
    }
    return channel_ptrs_.get();
  }

 private:
  THREAD_CHECKER(thread_checker_);
  std::unique_ptr<media::AudioBus> bus_;
  std::unique_ptr<float*[]> channel_ptrs_;
};

// Wraps AudioFifo to provide a cleaner interface to MediaStreamAudioProcessor.
// It avoids the FIFO when the source and destination frames match. All methods
// are called on one of the capture or render audio threads exclusively. If
// |source_channels| is larger than |destination_channels|, only the first
// |destination_channels| are kept from the source.
class MediaStreamAudioFifo {
 public:
  MediaStreamAudioFifo(int source_channels,
                       int destination_channels,
                       int source_frames,
                       int destination_frames,
                       int sample_rate)
      : source_channels_(source_channels),
        source_frames_(source_frames),
        sample_rate_(sample_rate),
        destination_(
            new MediaStreamAudioBus(destination_channels, destination_frames)),
        data_available_(false) {
    DCHECK_GE(source_channels, destination_channels);

    if (source_channels > destination_channels) {
      audio_source_intermediate_ =
          media::AudioBus::CreateWrapper(destination_channels);
    }

    if (source_frames != destination_frames) {
      // Since we require every Push to be followed by as many Consumes as
      // possible, twice the larger of the two is a (probably) loose upper bound
      // on the FIFO size.
      const int fifo_frames = 2 * std::max(source_frames, destination_frames);
      fifo_.reset(new media::AudioFifo(destination_channels, fifo_frames));
    }

    // May be created in the main render thread and used in the audio threads.
    DETACH_FROM_THREAD(thread_checker_);
  }

  void ReattachThreadChecker() {
    DETACH_FROM_THREAD(thread_checker_);
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    destination_->ReattachThreadChecker();
  }

  void Push(const media::AudioBus& source, base::TimeDelta audio_delay) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_EQ(source.channels(), source_channels_);
    DCHECK_EQ(source.frames(), source_frames_);

    const media::AudioBus* source_to_push = &source;

    if (audio_source_intermediate_) {
      for (int i = 0; i < destination_->bus()->channels(); ++i) {
        audio_source_intermediate_->SetChannelData(
            i, const_cast<float*>(source.channel(i)));
      }
      audio_source_intermediate_->set_frames(source.frames());
      source_to_push = audio_source_intermediate_.get();
    }

    if (fifo_) {
      CHECK_LT(fifo_->frames(), destination_->bus()->frames());
      next_audio_delay_ = audio_delay + fifo_->frames() *
                                            base::TimeDelta::FromSeconds(1) /
                                            sample_rate_;
      fifo_->Push(source_to_push);
    } else {
      CHECK(!data_available_);
      source_to_push->CopyTo(destination_->bus());
      next_audio_delay_ = audio_delay;
      data_available_ = true;
    }
  }

  // Returns true if there are destination_frames() of data available to be
  // consumed, and otherwise false.
  bool Consume(MediaStreamAudioBus** destination,
               base::TimeDelta* audio_delay) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (fifo_) {
      if (fifo_->frames() < destination_->bus()->frames())
        return false;

      fifo_->Consume(destination_->bus(), 0, destination_->bus()->frames());
      *audio_delay = next_audio_delay_;
      next_audio_delay_ -= destination_->bus()->frames() *
                           base::TimeDelta::FromSeconds(1) / sample_rate_;
    } else {
      if (!data_available_)
        return false;
      *audio_delay = next_audio_delay_;
      // The data was already copied to |destination_| in this case.
      data_available_ = false;
    }

    *destination = destination_.get();
    return true;
  }

 private:
  THREAD_CHECKER(thread_checker_);
  const int source_channels_;  // For a DCHECK.
  const int source_frames_;    // For a DCHECK.
  const int sample_rate_;
  std::unique_ptr<media::AudioBus> audio_source_intermediate_;
  std::unique_ptr<MediaStreamAudioBus> destination_;
  std::unique_ptr<media::AudioFifo> fifo_;

  // When using |fifo_|, this is the audio delay of the first sample to be
  // consumed next from the FIFO.  When not using |fifo_|, this is the audio
  // delay of the first sample in |destination_|.
  base::TimeDelta next_audio_delay_;

  // True when |destination_| contains the data to be returned by the next call
  // to Consume().  Only used when the FIFO is disabled.
  bool data_available_;
};

MediaStreamAudioProcessor::MediaStreamAudioProcessor(
    const blink::AudioProcessingProperties& properties,
    blink::WebRtcPlayoutDataSource* playout_data_source)
    : render_delay_ms_(0),
      audio_delay_stats_reporter_(kBuffersPerSecond),
      playout_data_source_(playout_data_source),
      main_thread_runner_(base::ThreadTaskRunnerHandle::Get()),
      audio_mirroring_(false),
      typing_detected_(false),
      aec_dump_agent_impl_(AecDumpAgentImpl::Create(this)),
      stopped_(false),
      use_capture_multi_channel_processing_(
          UseMultiChannelCaptureProcessing()) {
  DCHECK(main_thread_runner_);
  DETACH_FROM_THREAD(capture_thread_checker_);
  DETACH_FROM_THREAD(render_thread_checker_);

  InitializeAudioProcessingModule(properties);
}

MediaStreamAudioProcessor::~MediaStreamAudioProcessor() {
  // TODO(miu): This class is ref-counted, shared among threads, and then
  // requires itself to be destroyed on the main thread only?!?!? Fix this, and
  // then remove the hack in WebRtcAudioSink::Adapter.
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  Stop();
}

void MediaStreamAudioProcessor::OnCaptureFormatChanged(
    const media::AudioParameters& input_format) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  // There is no need to hold a lock here since the caller guarantees that
  // there is no more PushCaptureData() and ProcessAndConsumeData() callbacks
  // on the capture thread.
  InitializeCaptureFifo(input_format);

  // Reset the |capture_thread_checker_| since the capture data will come from
  // a new capture thread.
  DETACH_FROM_THREAD(capture_thread_checker_);
}

void MediaStreamAudioProcessor::PushCaptureData(
    const media::AudioBus& audio_source,
    base::TimeDelta capture_delay) {
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);
  TRACE_EVENT1("audio", "MediaStreamAudioProcessor::PushCaptureData",
               "delay (ms)", capture_delay.InMillisecondsF());
  capture_fifo_->Push(audio_source, capture_delay);
}

bool MediaStreamAudioProcessor::ProcessAndConsumeData(
    int volume,
    bool key_pressed,
    media::AudioBus** processed_data,
    base::TimeDelta* capture_delay,
    int* new_volume) {
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);
  DCHECK(processed_data);
  DCHECK(capture_delay);
  DCHECK(new_volume);

  TRACE_EVENT0("audio", "MediaStreamAudioProcessor::ProcessAndConsumeData");

  MediaStreamAudioBus* process_bus;
  if (!capture_fifo_->Consume(&process_bus, capture_delay))
    return false;

  // Use the process bus directly if audio processing is disabled.
  MediaStreamAudioBus* output_bus = process_bus;
  *new_volume = 0;
  if (audio_processing_) {
    output_bus = output_bus_.get();
    *new_volume = ProcessData(process_bus->channel_ptrs(),
                              process_bus->bus()->frames(), *capture_delay,
                              volume, key_pressed, output_bus->channel_ptrs());
  }

  // Swap channels before interleaving the data.
  if (audio_mirroring_ &&
      output_format_.channel_layout() == media::CHANNEL_LAYOUT_STEREO) {
    // Swap the first and second channels.
    output_bus->bus()->SwapChannels(0, 1);
  }

  *processed_data = output_bus->bus();

  return true;
}

void MediaStreamAudioProcessor::Stop() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  if (stopped_)
    return;

  stopped_ = true;

  aec_dump_agent_impl_.reset();

  if (!audio_processing_.get())
    return;

  blink::StopEchoCancellationDump(audio_processing_.get());
  worker_queue_.reset(nullptr);

  if (playout_data_source_) {
    playout_data_source_->RemovePlayoutSink(this);
    playout_data_source_ = nullptr;
  }
}

const media::AudioParameters& MediaStreamAudioProcessor::InputFormat() const {
  return input_format_;
}

const media::AudioParameters& MediaStreamAudioProcessor::OutputFormat() const {
  return output_format_;
}

void MediaStreamAudioProcessor::OnStartDump(base::File dump_file) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  DCHECK(dump_file.IsValid());

  if (audio_processing_) {
    if (!worker_queue_) {
      worker_queue_ = std::make_unique<rtc::TaskQueue>(
          CreateWebRtcTaskQueue(rtc::TaskQueue::Priority::LOW));
    }
    // Here tasks will be posted on the |worker_queue_|. It must be
    // kept alive until StopEchoCancellationDump is called or the
    // webrtc::AudioProcessing instance is destroyed.
    blink::StartEchoCancellationDump(audio_processing_.get(),
                                     std::move(dump_file), worker_queue_.get());
  } else {
    // Post the file close to avoid blocking the main thread.
    worker_pool::PostTask(
        FROM_HERE, {base::TaskPriority::LOWEST, base::MayBlock()},
        CrossThreadBindOnce([](base::File) {}, std::move(dump_file)));
  }
}

void MediaStreamAudioProcessor::OnStopDump() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  if (audio_processing_)
    blink::StopEchoCancellationDump(audio_processing_.get());

  // Note that deleting an rtc::TaskQueue has to be done from the
  // thread that created it.
  worker_queue_.reset(nullptr);
}

// static
bool MediaStreamAudioProcessor::WouldModifyAudio(
    const blink::AudioProcessingProperties& properties) {
  // Note: This method should by kept in-sync with any changes to the logic in
  // MediaStreamAudioProcessor::InitializeAudioProcessingModule().

  if (properties.goog_audio_mirroring)
    return true;

#if !defined(OS_IOS)
  if (properties.EchoCancellationIsWebRtcProvided() ||
      properties.goog_auto_gain_control) {
    return true;
  }
#endif

#if !defined(OS_IOS) && !defined(OS_ANDROID)
  if (properties.goog_experimental_echo_cancellation ||
      properties.goog_typing_noise_detection) {
    return true;
  }
#endif

  if (properties.goog_noise_suppression ||
      properties.goog_experimental_noise_suppression ||
      properties.goog_highpass_filter) {
    return true;
  }

  return false;
}

void MediaStreamAudioProcessor::OnPlayoutData(media::AudioBus* audio_bus,
                                              int sample_rate,
                                              int audio_delay_milliseconds) {
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
  DCHECK_GE(audio_bus->channels(), 1);
  DCHECK_LE(audio_bus->channels(), media::limits::kMaxChannels);
  int frames_per_10_ms = sample_rate / 100;
  if (audio_bus->frames() != frames_per_10_ms) {
    if (unsupported_buffer_size_log_count_ < 10) {
      LOG(ERROR) << "MSAP::OnPlayoutData: Unsupported audio buffer size "
                 << audio_bus->frames() << ", expected " << frames_per_10_ms;
      ++unsupported_buffer_size_log_count_;
    }
    return;
  }

  TRACE_EVENT1("audio", "MediaStreamAudioProcessor::OnPlayoutData",
               "delay (ms)", audio_delay_milliseconds);
  DCHECK_LT(audio_delay_milliseconds,
            std::numeric_limits<base::subtle::Atomic32>::max());
  base::subtle::Release_Store(&render_delay_ms_, audio_delay_milliseconds);

  webrtc::StreamConfig input_stream_config(sample_rate, audio_bus->channels());
  // If the input audio appears to contain upmixed mono audio, then APM is only
  // given the left channel. This reduces computational complexity and improves
  // convergence of audio processing algorithms.
  // TODO(crbug.com/1023337): Ensure correct channel count in input audio bus.
  assume_upmixed_mono_playout_ = assume_upmixed_mono_playout_ &&
                                 LeftAndRightChannelsAreSymmetric(*audio_bus);
  if (assume_upmixed_mono_playout_) {
    input_stream_config.set_num_channels(1);
  }
  std::array<const float*, media::limits::kMaxChannels> input_ptrs;
  for (int i = 0; i < static_cast<int>(input_stream_config.num_channels()); ++i)
    input_ptrs[i] = audio_bus->channel(i);

  // TODO(ajm): Should AnalyzeReverseStream() account for the
  // |audio_delay_milliseconds|?
  const int apm_error = audio_processing_->AnalyzeReverseStream(
      input_ptrs.data(), input_stream_config);
  if (apm_error != webrtc::AudioProcessing::kNoError &&
      apm_playout_error_code_log_count_ < 10) {
    LOG(ERROR) << "MSAP::OnPlayoutData: AnalyzeReverseStream error="
               << apm_error;
    ++apm_playout_error_code_log_count_;
  }
}

void MediaStreamAudioProcessor::OnPlayoutDataSourceChanged() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  // There is no need to hold a lock here since the caller guarantees that
  // there is no more OnPlayoutData() callback on the render thread.
  DETACH_FROM_THREAD(render_thread_checker_);
}

void MediaStreamAudioProcessor::OnRenderThreadChanged() {
  DETACH_FROM_THREAD(render_thread_checker_);
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
}

webrtc::AudioProcessorInterface::AudioProcessorStatistics
MediaStreamAudioProcessor::GetStats(bool has_remote_tracks) {
  AudioProcessorStatistics stats;
  stats.typing_noise_detected = base::subtle::Acquire_Load(&typing_detected_);
  stats.apm_statistics = audio_processing_->GetStatistics(has_remote_tracks);
  return stats;
}

void MediaStreamAudioProcessor::InitializeAudioProcessingModule(
    const blink::AudioProcessingProperties& properties) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DCHECK(!audio_processing_);

  // Note: The audio mirroring constraint (i.e., swap left and right channels)
  // is handled within this MediaStreamAudioProcessor and does not, by itself,
  // require webrtc::AudioProcessing.
  audio_mirroring_ = properties.goog_audio_mirroring;

#if defined(OS_ANDROID)
  const bool goog_experimental_aec = false;
  const bool goog_typing_detection = false;
#else
  const bool goog_experimental_aec =
      properties.goog_experimental_echo_cancellation;
  const bool goog_typing_detection = properties.goog_typing_noise_detection;
#endif

  // Return immediately if none of the goog constraints requiring
  // webrtc::AudioProcessing are enabled.
  if (!properties.EchoCancellationIsWebRtcProvided() &&
      !goog_experimental_aec && !properties.goog_noise_suppression &&
      !properties.goog_highpass_filter && !goog_typing_detection &&
      !properties.goog_auto_gain_control &&
      !properties.goog_experimental_noise_suppression) {
    // Sanity-check: WouldModifyAudio() should return true iff
    // |audio_mirroring_| is true.
    DCHECK_EQ(audio_mirroring_, WouldModifyAudio(properties));
    return;
  }

  // Sanity-check: WouldModifyAudio() should return true because the above logic
  // has determined webrtc::AudioProcessing will be used.
  DCHECK(WouldModifyAudio(properties));

  // Experimental options provided at creation.
  webrtc::Config config;
  config.Set<webrtc::ExperimentalNs>(new webrtc::ExperimentalNs(
      properties.goog_experimental_noise_suppression));

  // If the experimental AGC is enabled, check for overridden config params.
  if (properties.goog_experimental_auto_gain_control) {
    auto startup_min_volume = Platform::Current()->GetAgcStartupMinimumVolume();
    auto* experimental_agc =
        new webrtc::ExperimentalAgc(true, startup_min_volume.value_or(0));
    experimental_agc->digital_adaptive_disabled =
        base::FeatureList::IsEnabled(features::kWebRtcHybridAgc);

    config.Set<webrtc::ExperimentalAgc>(experimental_agc);
#if BUILDFLAG(IS_CHROMECAST)
  } else {
    config.Set<webrtc::ExperimentalAgc>(new webrtc::ExperimentalAgc(false));
#endif  // BUILDFLAG(IS_CHROMECAST)
  }

  // Create and configure the webrtc::AudioProcessing.
  base::Optional<std::string> audio_processing_platform_config_json =
      Platform::Current()->GetWebRTCAudioProcessingConfiguration();
  webrtc::AudioProcessingBuilder ap_builder;
  if (properties.EchoCancellationIsWebRtcProvided()) {
    webrtc::EchoCanceller3Config aec3_config;
    if (audio_processing_platform_config_json) {
      aec3_config = webrtc::Aec3ConfigFromJsonString(
          *audio_processing_platform_config_json);
      bool config_parameters_already_valid =
          webrtc::EchoCanceller3Config::Validate(&aec3_config);
      RTC_DCHECK(config_parameters_already_valid);
    }

    ap_builder.SetEchoControlFactory(
        std::unique_ptr<webrtc::EchoControlFactory>(
            new webrtc::EchoCanceller3Factory(aec3_config)));
  }
  audio_processing_.reset(ap_builder.Create(config));

  // Enable the audio processing components.
  if (playout_data_source_) {
    playout_data_source_->AddPlayoutSink(this);
  }

  webrtc::AudioProcessing::Config apm_config = audio_processing_->GetConfig();
  apm_config.pipeline.multi_channel_render = true;
  apm_config.pipeline.multi_channel_capture =
      use_capture_multi_channel_processing_;

  base::Optional<double> gain_control_compression_gain_db;
  blink::PopulateApmConfig(&apm_config, properties,
                           audio_processing_platform_config_json,
                           &gain_control_compression_gain_db);

  if (properties.goog_auto_gain_control ||
      properties.goog_experimental_auto_gain_control) {
    bool use_hybrid_agc = false;
    base::Optional<bool> use_peaks_not_rms;
    base::Optional<int> saturation_margin;
    if (properties.goog_experimental_auto_gain_control) {
      use_hybrid_agc = base::FeatureList::IsEnabled(features::kWebRtcHybridAgc);
      if (use_hybrid_agc) {
        DCHECK(properties.goog_auto_gain_control)
            << "Cannot enable hybrid AGC when AGC is disabled.";
      }
      use_peaks_not_rms = base::GetFieldTrialParamByFeatureAsBool(
          features::kWebRtcHybridAgc, "use_peaks_not_rms", false);
      saturation_margin = base::GetFieldTrialParamByFeatureAsInt(
          features::kWebRtcHybridAgc, "saturation_margin", -1);
    }
    blink::ConfigAutomaticGainControl(
        &apm_config, properties.goog_auto_gain_control,
        properties.goog_experimental_auto_gain_control, use_hybrid_agc,
        use_peaks_not_rms, saturation_margin, gain_control_compression_gain_db);
  }

  if (goog_typing_detection) {
    // TODO(xians): Remove this |typing_detector_| after the typing suppression
    // is enabled by default.
    typing_detector_.reset(new webrtc::TypingDetection());
    blink::EnableTypingDetection(&apm_config, typing_detector_.get());
  }

  // Ensure that 48 kHz APM processing is always active. This overrules the
  // default setting in WebRTC of 32 kHz for ARM platforms.
  if (Allow48kHzApmProcessing()) {
    apm_config.pipeline.maximum_internal_processing_rate = 48000;
  }

  apm_config.residual_echo_detector.enabled = false;
  audio_processing_->ApplyConfig(apm_config);
}

void MediaStreamAudioProcessor::InitializeCaptureFifo(
    const media::AudioParameters& input_format) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DCHECK(input_format.IsValid());
  input_format_ = input_format;

  // TODO(crbug/881275): For now, we assume fixed parameters for the output when
  // audio processing is enabled, to match the previous behavior. We should
  // either use the input parameters (in which case, audio processing will
  // convert at output) or ideally, have a backchannel from the sink to know
  // what format it would prefer.
  const int output_sample_rate = audio_processing_
                                     ?
#if BUILDFLAG(IS_CHROMECAST)
                                     std::min(blink::kAudioProcessingSampleRate,
                                              input_format.sample_rate())
#else
                                     blink::kAudioProcessingSampleRate
#endif  // BUILDFLAG(IS_CHROMECAST)
                                     : input_format.sample_rate();

  media::ChannelLayout output_channel_layout;
  if (!audio_processing_ || use_capture_multi_channel_processing_) {
    output_channel_layout = input_format.channel_layout();
  } else {
    output_channel_layout =
        media::GuessChannelLayout(kAudioProcessingNumberOfChannels);
  }

  // The output channels from the fifo is normally the same as input.
  int fifo_output_channels = input_format.channels();

  // Special case for if we have a keyboard mic channel on the input and no
  // audio processing is used. We will then have the fifo strip away that
  // channel. So we use stereo as output layout, and also change the output
  // channels for the fifo.
  if (input_format.channel_layout() ==
          media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC &&
      !audio_processing_) {
    output_channel_layout = media::CHANNEL_LAYOUT_STEREO;
    fifo_output_channels = ChannelLayoutToChannelCount(output_channel_layout);
  }

  // webrtc::AudioProcessing requires a 10 ms chunk size. We use this native
  // size when processing is enabled. When disabled we use the same size as
  // the source if less than 10 ms.
  //
  // TODO(ajm): This conditional buffer size appears to be assuming knowledge of
  // the sink based on the source parameters. PeerConnection sinks seem to want
  // 10 ms chunks regardless, while WebAudio sinks want less, and we're assuming
  // we can identify WebAudio sinks by the input chunk size. Less fragile would
  // be to have the sink actually tell us how much it wants (as in the above
  // todo).
  int processing_frames = input_format.sample_rate() / 100;
  int output_frames = output_sample_rate / 100;
  if (!audio_processing_ && input_format.frames_per_buffer() < output_frames) {
    processing_frames = input_format.frames_per_buffer();
    output_frames = processing_frames;
  }

  output_format_ = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, output_channel_layout,
      output_sample_rate, output_frames);

  capture_fifo_.reset(
      new MediaStreamAudioFifo(input_format.channels(), fifo_output_channels,
                               input_format.frames_per_buffer(),
                               processing_frames, input_format.sample_rate()));

  if (audio_processing_) {
    output_bus_.reset(
        new MediaStreamAudioBus(output_format_.channels(), output_frames));
  }
}

int MediaStreamAudioProcessor::ProcessData(const float* const* process_ptrs,
                                           int process_frames,
                                           base::TimeDelta capture_delay,
                                           int volume,
                                           bool key_pressed,
                                           float* const* output_ptrs) {
  DCHECK(audio_processing_);
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);

  base::subtle::Atomic32 render_delay_ms =
      base::subtle::Acquire_Load(&render_delay_ms_);
  int64_t capture_delay_ms = capture_delay.InMilliseconds();
  DCHECK_LT(capture_delay_ms,
            std::numeric_limits<base::subtle::Atomic32>::max());

  TRACE_EVENT2("audio", "MediaStreamAudioProcessor::ProcessData",
               "capture_delay_ms", capture_delay_ms, "render_delay_ms",
               render_delay_ms);

  const int total_delay_ms =
      static_cast<int>(capture_delay_ms) + render_delay_ms;
  if (total_delay_ms > 300 && large_delay_log_count_ < 10) {
    LOG(WARNING) << "Large audio delay, capture delay: " << capture_delay_ms
                 << "ms; render delay: " << render_delay_ms << "ms";
    ++large_delay_log_count_;
  }

  audio_delay_stats_reporter_.ReportDelay(
      capture_delay, base::TimeDelta::FromMilliseconds(render_delay_ms));

  webrtc::AudioProcessing* ap = audio_processing_.get();
  ap->set_stream_delay_ms(total_delay_ms);

  DCHECK_LE(volume, WebRtcAudioDeviceImpl::kMaxVolumeLevel);
  ap->set_stream_analog_level(volume);
  ap->set_stream_key_pressed(key_pressed);

  int err = ap->ProcessStream(process_ptrs, CreateStreamConfig(input_format_),
                              CreateStreamConfig(output_format_), output_ptrs);
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;

  if (typing_detector_) {
    // Ignore remote tracks to avoid unnecessary stats computation.
    auto voice_detected =
        ap->GetStatistics(false /* has_remote_tracks */).voice_detected;
    DCHECK(voice_detected.has_value());
    bool typing_detected =
        typing_detector_->Process(key_pressed, *voice_detected);
    base::subtle::Release_Store(&typing_detected_, typing_detected);
  }

  PostCrossThreadTask(
      *main_thread_runner_, FROM_HERE,
      CrossThreadBindOnce(&MediaStreamAudioProcessor::UpdateAecStats,
                          rtc::scoped_refptr<MediaStreamAudioProcessor>(this)));

  // Return 0 if the volume hasn't been changed, and otherwise the new volume.
  const int recommended_volume = ap->recommended_stream_analog_level();
  return (recommended_volume == volume) ? 0 : recommended_volume;
}

void MediaStreamAudioProcessor::UpdateAecStats() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
}

}  // namespace blink
