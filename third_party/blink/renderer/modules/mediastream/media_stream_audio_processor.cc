// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_audio_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/webrtc/constants.h"
#include "media/webrtc/helpers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/mediastream/aec_dump_agent_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing_statistics.h"
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

using EchoCancellationType = AudioProcessingProperties::EchoCancellationType;

namespace {

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
    bus_->Zero();
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
      fifo_ =
          std::make_unique<media::AudioFifo>(destination_channels, fifo_frames);
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
      next_audio_delay_ =
          audio_delay + fifo_->frames() * base::Seconds(1) / sample_rate_;
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
      next_audio_delay_ -=
          destination_->bus()->frames() * base::Seconds(1) / sample_rate_;
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
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback,
    const AudioProcessingProperties& properties,
    bool use_capture_multi_channel_processing,
    scoped_refptr<WebRtcAudioDeviceImpl> playout_data_source)
    : deliver_processed_audio_callback_(deliver_processed_audio_callback),
      log_callback_(log_callback),
      render_delay_(base::TimeDelta()),
      audio_delay_stats_reporter_(kBuffersPerSecond),
      playout_data_source_(std::move(playout_data_source)),
      main_thread_runner_(base::ThreadTaskRunnerHandle::Get()),
      audio_mirroring_(false),
      aec_dump_agent_impl_(AecDumpAgentImpl::Create(this)),
      stopped_(false),
      use_capture_multi_channel_processing_(
          use_capture_multi_channel_processing) {
  DCHECK(deliver_processed_audio_callback_);
  DCHECK(log_callback_);
  DCHECK(main_thread_runner_);
  DETACH_FROM_THREAD(capture_thread_checker_);
  DETACH_FROM_THREAD(render_thread_checker_);
  SendLogMessage(base::StringPrintf(
      "%s({use_capture_multi_channel_processing=%s})", __func__,
      use_capture_multi_channel_processing ? "true" : "false"));

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

  InitializeCaptureFifo(input_format);

  // Reset the |capture_thread_checker_| since the capture data will come from
  // a new capture thread.
  DETACH_FROM_THREAD(capture_thread_checker_);
}

void MediaStreamAudioProcessor::ProcessCapturedAudio(
    const media::AudioBus& audio_source,
    base::TimeTicks audio_capture_time,
    int num_preferred_channels,
    double volume,
    bool key_pressed) {
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);
  DCHECK(deliver_processed_audio_callback_);
  // Sanity-check the input audio format in debug builds.
  DCHECK(input_format_.IsValid());
  DCHECK_EQ(audio_source.channels(), input_format_.channels());
  DCHECK_EQ(audio_source.frames(), input_format_.frames_per_buffer());

  base::TimeDelta capture_delay = base::TimeTicks::Now() - audio_capture_time;
  TRACE_EVENT1("audio", "MediaStreamAudioProcessor::ProcessCapturedAudio",
               "delay (ms)", capture_delay.InMillisecondsF());

  capture_fifo_->Push(audio_source, capture_delay);

  // Process and consume the data in the FIFO until there is not enough
  // data to process.
  MediaStreamAudioBus* process_bus;
  while (capture_fifo_->Consume(&process_bus, &capture_delay)) {
    // Use the process bus directly if audio processing is disabled.
    MediaStreamAudioBus* output_bus = process_bus;
    absl::optional<double> new_volume;
    if (audio_processing_) {
      output_bus = output_bus_.get();
      new_volume =
          ProcessData(process_bus->channel_ptrs(), process_bus->bus()->frames(),
                      capture_delay, volume, key_pressed,
                      num_preferred_channels, output_bus->channel_ptrs());
    }

    // Swap channels before interleaving the data.
    if (audio_mirroring_ &&
        output_format_.channel_layout() == media::CHANNEL_LAYOUT_STEREO) {
      // Swap the first and second channels.
      output_bus->bus()->SwapChannels(0, 1);
    }

    deliver_processed_audio_callback_.Run(*output_bus->bus(),
                                          audio_capture_time, new_volume);
  }
}

void MediaStreamAudioProcessor::Stop() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  if (stopped_)
    return;

  stopped_ = true;

  deliver_processed_audio_callback_.Reset();
  aec_dump_agent_impl_.reset();

  if (!audio_processing_.get())
    return;

  media::StopEchoCancellationDump(audio_processing_.get());
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

void MediaStreamAudioProcessor::SetOutputWillBeMuted(bool muted) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DCHECK(base::FeatureList::IsEnabled(
      features::kMinimizeAudioProcessingForUnusedOutput));
  SendLogMessage(
      base::StringPrintf("%s({muted=%s})", __func__, muted ? "true" : "false"));
  if (audio_processing_) {
    audio_processing_->set_output_will_be_muted(muted);
  }
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
    // kept alive until media::StopEchoCancellationDump is called or the
    // webrtc::AudioProcessing instance is destroyed.
    media::StartEchoCancellationDump(audio_processing_.get(),
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
    media::StopEchoCancellationDump(audio_processing_.get());

  // Note that deleting an rtc::TaskQueue has to be done from the
  // thread that created it.
  worker_queue_.reset(nullptr);
}

// static
bool MediaStreamAudioProcessor::WouldModifyAudio(
    const AudioProcessingProperties& properties) {
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
  if (properties.goog_experimental_echo_cancellation) {
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
                                              base::TimeDelta audio_delay) {
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
               "delay (ms)", audio_delay.InMillisecondsF());
  render_delay_ = audio_delay;

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

  // TODO(ajm): Should AnalyzeReverseStream() account for the |audio_delay|?
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
  DETACH_FROM_THREAD(render_thread_checker_);
}

void MediaStreamAudioProcessor::OnRenderThreadChanged() {
  DETACH_FROM_THREAD(render_thread_checker_);
  DCHECK_CALLED_ON_VALID_THREAD(render_thread_checker_);
}

webrtc::AudioProcessorInterface::AudioProcessorStatistics
MediaStreamAudioProcessor::GetStats(bool has_remote_tracks) {
  AudioProcessorStatistics stats;
  stats.apm_statistics = audio_processing_->GetStatistics(has_remote_tracks);
  return stats;
}

void MediaStreamAudioProcessor::InitializeAudioProcessingModule(
    const AudioProcessingProperties& properties) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DCHECK(!audio_processing_);
  SendLogMessage(base::StringPrintf("%s()", __func__));

  // Note: The audio mirroring constraint (i.e., swap left and right channels)
  // is handled within this MediaStreamAudioProcessor and does not, by itself,
  // require webrtc::AudioProcessing.
  audio_mirroring_ = properties.goog_audio_mirroring;

#if defined(OS_ANDROID)
  const bool goog_experimental_aec = false;
#else
  const bool goog_experimental_aec =
      properties.goog_experimental_echo_cancellation;
#endif

  // Return immediately if none of the goog constraints requiring
  // webrtc::AudioProcessing are enabled.
  if (!properties.EchoCancellationIsWebRtcProvided() &&
      !goog_experimental_aec && !properties.goog_noise_suppression &&
      !properties.goog_highpass_filter && !properties.goog_auto_gain_control &&
      !properties.goog_experimental_noise_suppression) {
    // Sanity-check: WouldModifyAudio() should return true iff
    // |audio_mirroring_| is true.
    DCHECK_EQ(audio_mirroring_, WouldModifyAudio(properties));
    return;
  }

  // Sanity-check: WouldModifyAudio() should return true because the above logic
  // has determined webrtc::AudioProcessing will be used.
  DCHECK(WouldModifyAudio(properties));

  audio_processing_ = media::CreateWebRtcAudioProcessingModule(
      properties.ToAudioProcessingSettings(
          use_capture_multi_channel_processing_));

  // Register as a listener for the echo cancellation playout reference signal.
  if (playout_data_source_) {
    playout_data_source_->AddPlayoutSink(this);
  }
}

void MediaStreamAudioProcessor::InitializeCaptureFifo(
    const media::AudioParameters& input_format) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
  DCHECK(input_format.IsValid());
  SendLogMessage(
      base::StringPrintf("%s({input_format=[%s]})", __func__,
                         input_format.AsHumanReadableString().c_str()));

  input_format_ = input_format;

  // TODO(crbug/881275): For now, we assume fixed parameters for the output when
  // audio processing is enabled, to match the previous behavior. We should
  // either use the input parameters (in which case, audio processing will
  // convert at output) or ideally, have a backchannel from the sink to know
  // what format it would prefer.
  const int output_sample_rate =
      audio_processing_ ?
#if BUILDFLAG(IS_CHROMECAST)
                        std::min(media::kAudioProcessingSampleRateHz,
                                 input_format.sample_rate())
#else
                        media::kAudioProcessingSampleRateHz
#endif  // BUILDFLAG(IS_CHROMECAST)
                        : input_format.sample_rate();

  // The output channels from the fifo is normally the same as input.
  int fifo_output_channels = input_format.channels();

  media::ChannelLayout output_channel_layout;
  if (!audio_processing_) {
    if (input_format.channel_layout() ==
        media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
      // Special case for if we have a keyboard mic channel on the input and no
      // audio processing is used. We will then have the fifo strip away that
      // channel. So we use stereo as output layout, and also change the output
      // channels for the fifo.
      output_channel_layout = media::CHANNEL_LAYOUT_STEREO;
      fifo_output_channels = ChannelLayoutToChannelCount(output_channel_layout);
    } else {
      output_channel_layout = input_format.channel_layout();
    }
  } else if (use_capture_multi_channel_processing_) {
    // The number of output channels is equal to the number of input channels.
    // If the media stream audio processor receives stereo input it will output
    // stereo. To reduce computational complexity, APM will not perform full
    // multichannel processing unless any sink requests more than one channel.
    // If the input is multichannel but the sinks are not interested in more
    // than one channel, APM will internally downmix the signal to mono and
    // process it. The processed mono signal will then be upmixed to same number
    // of channels as the input before leaving the media stream audio processor.
    // If a sink later requests stereo, APM will start performing true stereo
    // processing. There will be no need to change the output format.

    // The keyboard mic channel shall not be part of the output.
    if (input_format.channel_layout() ==
        media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
      output_channel_layout = media::CHANNEL_LAYOUT_STEREO;
    } else {
      output_channel_layout = input_format.channel_layout();
    }
  } else {
    output_channel_layout = media::CHANNEL_LAYOUT_MONO;
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
  if (output_channel_layout == media::CHANNEL_LAYOUT_DISCRETE) {
    // Explicitly set number of channels for discrete channel layouts.
    output_format_.set_channels_for_discrete(input_format.channels());
  }
  SendLogMessage(
      base::StringPrintf("%s => (output_format=[%s])", __func__,
                         output_format_.AsHumanReadableString().c_str()));
  SendLogMessage(base::StringPrintf(
      "%s => (FIFO: processing_frames=%d, output_channels=%d)", __func__,
      processing_frames, fifo_output_channels));

  capture_fifo_ = std::make_unique<MediaStreamAudioFifo>(
      input_format.channels(), fifo_output_channels,
      input_format.frames_per_buffer(), processing_frames,
      input_format.sample_rate());

  if (audio_processing_) {
    output_bus_ = std::make_unique<MediaStreamAudioBus>(
        output_format_.channels(), output_frames);
  }
}

absl::optional<double> MediaStreamAudioProcessor::ProcessData(
    const float* const* process_ptrs,
    int process_frames,
    base::TimeDelta capture_delay,
    double volume,
    bool key_pressed,
    int num_preferred_channels,
    float* const* output_ptrs) {
  DCHECK(audio_processing_);
  DCHECK_CALLED_ON_VALID_THREAD(capture_thread_checker_);

  const base::TimeDelta render_delay = render_delay_;

  TRACE_EVENT2("audio", "MediaStreamAudioProcessor::ProcessData",
               "capture_delay (ms)", capture_delay.InMillisecondsF(),
               "render_delay (ms)", render_delay.InMillisecondsF());

  const int64_t total_delay_ms =
      (capture_delay + render_delay).InMilliseconds();

  if (total_delay_ms > 300 && large_delay_log_count_ < 10) {
    LOG(WARNING) << "Large audio delay, capture delay: "
                 << capture_delay.InMillisecondsF()
                 << "ms; render delay: " << render_delay.InMillisecondsF()
                 << "ms";
    ++large_delay_log_count_;
  }

  audio_delay_stats_reporter_.ReportDelay(capture_delay, render_delay);

  webrtc::AudioProcessing* ap = audio_processing_.get();
  DCHECK_LE(total_delay_ms, std::numeric_limits<int>::max());
  ap->set_stream_delay_ms(base::saturated_cast<int>(total_delay_ms));

  // Keep track of the maximum number of preferred channels. The number of
  // output channels of APM can increase if preferred by the sinks, but
  // never decrease.
  max_num_preferred_output_channels_ =
      std::max(max_num_preferred_output_channels_, num_preferred_channels);

  // Upscale the volume to the range expected by the WebRTC automatic gain
  // controller.
#if defined(OS_WIN) || defined(OS_MAC)
  DCHECK_LE(volume, 1.0);
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || defined(OS_OPENBSD)
  // We have a special situation on Linux where the microphone volume can be
  // "higher than maximum". The input volume slider in the sound preference
  // allows the user to set a scaling that is higher than 100%. It means that
  // even if the reported maximum levels is N, the actual microphone level can
  // go up to 1.5x*N and that corresponds to a normalized |volume| of 1.5x.
  DCHECK_LE(volume, 1.6);
#endif
  // Map incoming volume range of [0.0, 1.0] to [0, 255] used by AGC.
  // The volume can be higher than 255 on Linux, and it will be cropped to
  // 255 since AGC does not allow values out of range.
  const int max_analog_gain_level = media::MaxWebRtcAnalogGainLevel();
  int current_analog_gain_level =
      static_cast<int>((volume * max_analog_gain_level) + 0.5);
  current_analog_gain_level =
      std::min(current_analog_gain_level, max_analog_gain_level);
  DCHECK_LE(current_analog_gain_level, max_analog_gain_level);

  ap->set_stream_analog_level(current_analog_gain_level);
  ap->set_stream_key_pressed(key_pressed);

  // Depending on how many channels the sinks prefer, the number of APM output
  // channels is allowed to vary between 1 and the number of channels of the
  // output format. The output format in turn depends on the input format.
  // Example: With a stereo mic the output format will have 2 channels, and APM
  // will produce 1 or 2 output channels depending on the sinks.
  int num_apm_output_channels =
      std::min(max_num_preferred_output_channels_, output_format_.channels());

  // Limit number of apm output channels to 2 to avoid potential problems with
  // discrete channel mapping.
  num_apm_output_channels = std::min(num_apm_output_channels, 2);

  CHECK_GE(num_apm_output_channels, 1);
  const webrtc::StreamConfig apm_output_config = webrtc::StreamConfig(
      output_format_.sample_rate(), num_apm_output_channels, false);

  int err = ap->ProcessStream(process_ptrs, CreateStreamConfig(input_format_),
                              apm_output_config, output_ptrs);
  DCHECK_EQ(err, 0) << "ProcessStream() error: " << err;

  // Upmix if the number of channels processed by APM is less than the number
  // specified in the output format. Channels above stereo will be set to zero.
  if (num_apm_output_channels < output_format_.channels()) {
    if (num_apm_output_channels == 1) {
      // The right channel is a copy of the left channel. Remaining channels
      // have already been set to zero at initialization.
      memcpy(&output_ptrs[1][0], &output_ptrs[0][0],
             output_format_.frames_per_buffer() * sizeof(output_ptrs[0][0]));
    }
  }

  PostCrossThreadTask(
      *main_thread_runner_, FROM_HERE,
      CrossThreadBindOnce(&MediaStreamAudioProcessor::UpdateAecStats,
                          rtc::scoped_refptr<MediaStreamAudioProcessor>(this)));

  // Return a new mic volume, if the volume has been changed.
  const int recommended_analog_gain_level =
      ap->recommended_stream_analog_level();
  if (recommended_analog_gain_level == current_analog_gain_level) {
    return absl::nullopt;
  } else {
    return static_cast<double>(recommended_analog_gain_level) /
           media::MaxWebRtcAnalogGainLevel();
  }
}

void MediaStreamAudioProcessor::UpdateAecStats() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
}

void MediaStreamAudioProcessor::SendLogMessage(const std::string& message) {
  log_callback_.Run(base::StringPrintf("MSAP::%s [this=0x%" PRIXPTR "]",
                                       message.c_str(),
                                       reinterpret_cast<uintptr_t>(this)));
}

}  // namespace blink
