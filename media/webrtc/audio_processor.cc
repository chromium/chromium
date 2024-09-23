// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/webrtc/audio_processor.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/webrtc/constants.h"
#include "media/webrtc/helpers.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace media {
namespace {
constexpr int kBuffersPerSecond = 100;  // 10 ms per buffer.

int GetCaptureBufferSize(bool need_webrtc_processing,
                         const AudioParameters device_format) {
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CAST_ANDROID)
  // TODO(henrika): Re-evaluate whether to use same logic as other platforms.
  // https://crbug.com/638081
  // Note: This computation does not match 2x10 ms as defined for audio
  // processing when rates are 50 modulo 100. 22050 Hz here gives buffer size
  // (2*22050)/100 = 441 samples, while WebRTC processes in chunks of 22050/100
  // = 220 samples. This leads to unnecessary rebuffering.
  return 2 * device_format.sample_rate() / 100;
#else
  const int buffer_size_10_ms = device_format.sample_rate() / 100;
  // If audio processing is turned on, require 10ms buffers to avoid
  // rebuffering.
  if (need_webrtc_processing) {
    DCHECK_EQ(buffer_size_10_ms, webrtc::AudioProcessing::GetFrameSize(
                                     device_format.sample_rate()));
    return buffer_size_10_ms;
  }

  // If WebRTC audio processing is not required and the native hardware buffer
  // size was provided, use it. It can be harmful, in terms of CPU/power
  // consumption, to use smaller buffer sizes than the native size.
  // (https://crbug.com/362261).
  if (int hardware_buffer_size = device_format.frames_per_buffer())
    return hardware_buffer_size;

  // If the buffer size is missing from the device parameters, provide 10ms as
  // a fall-back.
  return buffer_size_10_ms;
#endif
}

bool ApmNeedsPlayoutReference(const webrtc::AudioProcessing* apm,
                              const AudioProcessingSettings& settings) {
  if (!base::FeatureList::IsEnabled(
          features::kWebRtcApmTellsIfPlayoutReferenceIsNeeded)) {
    return settings.NeedPlayoutReference();
  }
  if (!apm) {
    // APM is not available; hence, observing the playout reference is not
    // needed.
    return false;
  }
  // TODO(crbug.com/40889535): Move the logic below into WebRTC APM since APM
  // may use injected sub-modules the usage of which is not reflected in the APM
  // config (e.g., render side processing).
  const webrtc::AudioProcessing::Config config = apm->GetConfig();
  const bool aec = config.echo_canceller.enabled;
  const bool legacy_agc =
      config.gain_controller1.enabled &&
      !config.gain_controller1.analog_gain_controller.enabled;
  return aec || legacy_agc;
}
}  // namespace

// Wraps AudioBus to provide access to the array of channel pointers, since this
// is the type webrtc::AudioProcessing deals in. The array is refreshed on every
// channel_ptrs() call, and will be valid until the underlying AudioBus pointers
// are changed, e.g. through calls to SetChannelData() or SwapChannels().
class AudioProcessorCaptureBus {
 public:
  AudioProcessorCaptureBus(int channels, int frames)
      : bus_(media::AudioBus::Create(channels, frames)),
        channel_ptrs_(new float*[channels]) {
    bus_->Zero();
  }

  media::AudioBus* bus() { return bus_.get(); }

  float* const* channel_ptrs() {
    for (int i = 0; i < bus_->channels(); ++i) {
      channel_ptrs_[i] = bus_->channel(i);
    }
    return channel_ptrs_.get();
  }

 private:
  std::unique_ptr<media::AudioBus> bus_;
  std::unique_ptr<float*[]> channel_ptrs_;
};

// Wraps AudioFifo to provide a cleaner interface to AudioProcessor.
// It avoids the FIFO when the source and destination frames match. If
// |source_channels| is larger than |destination_channels|, only the first
// |destination_channels| are kept from the source.
// Does not support concurrent access.
class AudioProcessorCaptureFifo {
 public:
  AudioProcessorCaptureFifo(int source_channels,
                            int destination_channels,
                            int source_frames,
                            int destination_frames,
                            int sample_rate)
      :
#if DCHECK_IS_ON()
        source_channels_(source_channels),
        source_frames_(source_frames),
#endif
        sample_rate_(sample_rate),
        destination_(
            std::make_unique<AudioProcessorCaptureBus>(destination_channels,
                                                       destination_frames)),
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
  }

  void Push(const media::AudioBus& source, base::TimeDelta audio_delay) {
#if DCHECK_IS_ON()
    DCHECK_EQ(source.channels(), source_channels_);
    DCHECK_EQ(source.frames(), source_frames_);
#endif
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
  bool Consume(AudioProcessorCaptureBus** destination,
               base::TimeDelta* audio_delay) {
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
#if DCHECK_IS_ON()
  const int source_channels_;
  const int source_frames_;
#endif
  const int sample_rate_;
  std::unique_ptr<media::AudioBus> audio_source_intermediate_;
  std::unique_ptr<AudioProcessorCaptureBus> destination_;
  std::unique_ptr<media::AudioFifo> fifo_;

  // When using |fifo_|, this is the audio delay of the first sample to be
  // consumed next from the FIFO.  When not using |fifo_|, this is the audio
  // delay of the first sample in |destination_|.
  base::TimeDelta next_audio_delay_;

  // True when |destination_| contains the data to be returned by the next call
  // to Consume().  Only used when the FIFO is disabled.
  bool data_available_;
};

// static
std::unique_ptr<AudioProcessor> AudioProcessor::Create(
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback,
    const AudioProcessingSettings& settings,
    const media::AudioParameters& input_format,
    const media::AudioParameters& output_format) {
  log_callback.Run(base::StringPrintf(
      "AudioProcessor::Create({multi_channel_capture_processing=%s})",
      settings.multi_channel_capture_processing ? "true" : "false"));

  rtc::scoped_refptr<webrtc::AudioProcessing> webrtc_audio_processing =
      media::CreateWebRtcAudioProcessingModule(settings);

  return std::make_unique<AudioProcessor>(
      std::move(deliver_processed_audio_callback), std::move(log_callback),
      input_format, output_format, std::move(webrtc_audio_processing),
      settings.stereo_mirroring,
      ApmNeedsPlayoutReference(webrtc_audio_processing.get(), settings));
}

AudioProcessor::AudioProcessor(
    DeliverProcessedAudioCallback deliver_processed_audio_callback,
    LogCallback log_callback,
    const media::AudioParameters& input_format,
    const media::AudioParameters& output_format,
    rtc::scoped_refptr<webrtc::AudioProcessing> webrtc_audio_processing,
    bool stereo_mirroring,
    bool needs_playout_reference)
    : webrtc_audio_processing_(webrtc_audio_processing),
      stereo_mirroring_(stereo_mirroring),
      needs_playout_reference_(needs_playout_reference),
      log_callback_(std::move(log_callback)),
      input_format_(input_format),
      output_format_(output_format),
      deliver_processed_audio_callback_(
          std::move(deliver_processed_audio_callback)),
      audio_delay_stats_reporter_(kBuffersPerSecond),
      playout_fifo_(
          // Unretained is safe, since the callback is always called
          // synchronously within the AudioProcessor.
          base::BindRepeating(&AudioProcessor::AnalyzePlayoutData,
                              base::Unretained(this))) {
  DCHECK(deliver_processed_audio_callback_);
  DCHECK(log_callback_);

  CHECK(input_format_.IsValid());
  CHECK(output_format_.IsValid());
  if (webrtc_audio_processing_) {
    DCHECK_EQ(
        webrtc::AudioProcessing::GetFrameSize(output_format_.sample_rate()),
        output_format_.frames_per_buffer());
  }
  if (input_format_.sample_rate() % 100 != 0 ||
      output_format_.sample_rate() % 100 != 0) {
    // The WebRTC audio processing module may simulate clock drift on
    // non-divisible sample rates.
    SendLogMessage(base::StringPrintf(
        "%s: WARNING: Sample rate not divisible by 100, processing is provided "
        "on a best-effort basis. input rate=[%d], output rate=[%d]",
        __func__, input_format_.sample_rate(), output_format_.sample_rate()));
  }
  SendLogMessage(base::StringPrintf(
      "%s({input_format_=[%s], output_format_=[%s]})", __func__,
      input_format_.AsHumanReadableString().c_str(),
      output_format_.AsHumanReadableString().c_str()));

  // If audio processing is needed, rebuffer to APM frame size. If not, rebuffer
  // to the requested output format.
  const int fifo_output_frames_per_buffer =
      webrtc_audio_processing_
          ? webrtc::AudioProcessing::GetFrameSize(input_format_.sample_rate())
          : output_format_.frames_per_buffer();
  SendLogMessage(base::StringPrintf(
      "%s => (capture FIFO: fifo_output_frames_per_buffer=%d)", __func__,
      fifo_output_frames_per_buffer));
  capture_fifo_ = std::make_unique<AudioProcessorCaptureFifo>(
      input_format.channels(), input_format_.channels(),
      input_format.frames_per_buffer(), fifo_output_frames_per_buffer,
      input_format.sample_rate());

  if (webrtc_audio_processing_) {
    output_bus_ = std::make_unique<AudioProcessorCaptureBus>(
        output_format_.channels(), output_format.frames_per_buffer());
  }
}

AudioProcessor::~AudioProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  OnStopDump();
}

void AudioProcessor::ProcessCapturedAudio(const media::AudioBus& audio_source,
                                          base::TimeTicks audio_capture_time,
                                          int num_preferred_channels,
                                          double volume,
                                          bool key_pressed) {
  DCHECK(deliver_processed_audio_callback_);
  // Sanity-check the input audio format in debug builds.
  DCHECK(input_format_.IsValid());
  DCHECK_EQ(audio_source.channels(), input_format_.channels());
  DCHECK_EQ(audio_source.frames(), input_format_.frames_per_buffer());

  base::TimeDelta capture_delay = base::TimeTicks::Now() - audio_capture_time;
  TRACE_EVENT("audio", "AudioProcessor::ProcessCapturedAudio",
              "capture_time (ms)",
              (audio_capture_time - base::TimeTicks()).InMillisecondsF(),
              "capture_delay (ms)", capture_delay.InMillisecondsF());

  capture_fifo_->Push(audio_source, capture_delay);

  // Process and consume the data in the FIFO until there is not enough
  // data to process.
  AudioProcessorCaptureBus* process_bus;
  while (capture_fifo_->Consume(&process_bus, &capture_delay)) {
    // Use the process bus directly if audio processing is disabled.
    AudioProcessorCaptureBus* output_bus = process_bus;
    std::optional<double> new_volume;
    if (webrtc_audio_processing_) {
      output_bus = output_bus_.get();
      new_volume =
          ProcessData(process_bus->channel_ptrs(), process_bus->bus()->frames(),
                      capture_delay, volume, key_pressed,
                      num_preferred_channels, output_bus->channel_ptrs());
    }

    // Swap channels before interleaving the data.
    if (stereo_mirroring_ &&
        output_format_.channel_layout() == media::CHANNEL_LAYOUT_STEREO) {
      // Swap the first and second channels.
      output_bus->bus()->SwapChannels(0, 1);
    }

    deliver_processed_audio_callback_.Run(*output_bus->bus(),
                                          audio_capture_time, new_volume);
  }
}

void AudioProcessor::SetOutputWillBeMuted(bool muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  SendLogMessage(
      base::StringPrintf("%s({muted=%s})", __func__, muted ? "true" : "false"));
  if (webrtc_audio_processing_) {
    webrtc_audio_processing_->set_output_will_be_muted(muted);
  }
}

void AudioProcessor::OnStartDump(base::File dump_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(dump_file.IsValid());

  if (webrtc_audio_processing_) {
    if (!worker_queue_) {
      worker_queue_ =
          CreateWebRtcTaskQueue(webrtc::TaskQueueFactory::Priority::LOW);
    }
    // Here tasks will be posted on the |worker_queue_|. It must be
    // kept alive until media::StopEchoCancellationDump is called or the
    // webrtc::AudioProcessing instance is destroyed.
    media::StartEchoCancellationDump(webrtc_audio_processing_.get(),
                                     std::move(dump_file), worker_queue_.get());
  } else {
    // Post the file close to avoid blocking the control sequence.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::LOWEST, base::MayBlock()},
        base::DoNothingWithBoundArgs(std::move(dump_file)));
  }
}

void AudioProcessor::OnStopDump() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!worker_queue_)
    return;
  if (webrtc_audio_processing_)
    media::StopEchoCancellationDump(webrtc_audio_processing_.get());
  worker_queue_ = nullptr;
}

void AudioProcessor::OnPlayoutData(const AudioBus& audio_bus,
                                   int sample_rate,
                                   base::TimeDelta audio_delay) {
  TRACE_EVENT1("audio", "AudioProcessor::OnPlayoutData", "playout_delay (ms)",
               audio_delay.InMillisecondsF());

  if (!webrtc_audio_processing_) {
    return;
  }

  unbuffered_playout_delay_ = audio_delay;

  if (!playout_sample_rate_hz_ || sample_rate != *playout_sample_rate_hz_) {
    // We reset the buffer on sample rate changes because the current buffer
    // content is rendered obsolete (the audio processing module will reset
    // internally) and the FIFO does not resample previous content to the new
    // rate.
    // Channel count changes are already handled within the AudioPushFifo.
    playout_sample_rate_hz_ = sample_rate;
    const int samples_per_channel =
        webrtc::AudioProcessing::GetFrameSize(sample_rate);
    playout_fifo_.Reset(samples_per_channel);
  }

  playout_fifo_.Push(audio_bus);
}

void AudioProcessor::AnalyzePlayoutData(const AudioBus& audio_bus,
                                        int frame_delay) {
  DCHECK(webrtc_audio_processing_);
  DCHECK(playout_sample_rate_hz_.has_value());

  const base::TimeDelta playout_delay =
      unbuffered_playout_delay_ +
      AudioTimestampHelper::FramesToTime(frame_delay, *playout_sample_rate_hz_);
  playout_delay_ = playout_delay;
  TRACE_EVENT("audio", "AudioProcessor::AnalyzePlayoutData", "delay (frames)",
              frame_delay, "playout_delay (ms)",
              playout_delay.InMillisecondsF());

  webrtc::StreamConfig input_stream_config(*playout_sample_rate_hz_,
                                           audio_bus.channels());
  std::array<const float*, media::limits::kMaxChannels> input_ptrs;
  for (int i = 0; i < audio_bus.channels(); ++i)
    input_ptrs[i] = audio_bus.channel(i);

  const int apm_error = webrtc_audio_processing_->AnalyzeReverseStream(
      input_ptrs.data(), input_stream_config);
  if (apm_error != webrtc::AudioProcessing::kNoError &&
      apm_playout_error_code_log_count_ < 10) {
    LOG(ERROR) << "MSAP::OnPlayoutData: AnalyzeReverseStream error="
               << apm_error;
    ++apm_playout_error_code_log_count_;
  }
}

webrtc::AudioProcessingStats AudioProcessor::GetStats() {
  if (!webrtc_audio_processing_)
    return {};
  return webrtc_audio_processing_->GetStatistics();
}

std::optional<double> AudioProcessor::ProcessData(
    const float* const* process_ptrs,
    int process_frames,
    base::TimeDelta capture_delay,
    double volume,
    bool key_pressed,
    int num_preferred_channels,
    float* const* output_ptrs) {
  DCHECK(webrtc_audio_processing_);

  const base::TimeDelta playout_delay = playout_delay_;

  TRACE_EVENT2("audio", "AudioProcessor::ProcessData", "capture_delay (ms)",
               capture_delay.InMillisecondsF(), "playout_delay (ms)",
               playout_delay.InMillisecondsF());

  const int64_t total_delay_ms =
      (capture_delay + playout_delay).InMilliseconds();

  if (total_delay_ms > 300 && large_delay_log_count_ < 10) {
    LOG(WARNING) << "Large audio delay, capture delay: "
                 << capture_delay.InMillisecondsF()
                 << "ms; playout delay: " << playout_delay.InMillisecondsF()
                 << "ms";
    ++large_delay_log_count_;
  }

  audio_delay_stats_reporter_.ReportDelay(capture_delay, playout_delay);

  webrtc::AudioProcessing* ap = webrtc_audio_processing_.get();
  DCHECK_LE(total_delay_ms, std::numeric_limits<int>::max());
  ap->set_stream_delay_ms(base::saturated_cast<int>(total_delay_ms));

  // Keep track of the maximum number of preferred channels. The number of
  // output channels of APM can increase if preferred by the sinks, but
  // never decrease.
  max_num_preferred_output_channels_ =
      std::max(max_num_preferred_output_channels_, num_preferred_channels);

  // Upscale the volume to the range expected by the WebRTC automatic gain
  // controller.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  DCHECK_LE(volume, 1.0);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
    BUILDFLAG(IS_OPENBSD)
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
      output_format_.sample_rate(), num_apm_output_channels);

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

  // Return a new mic volume, if the volume has been changed.
  const int recommended_analog_gain_level =
      ap->recommended_stream_analog_level();
  if (recommended_analog_gain_level == current_analog_gain_level) {
    return std::nullopt;
  } else {
    return static_cast<double>(recommended_analog_gain_level) /
           media::MaxWebRtcAnalogGainLevel();
  }
}

// Called on the owning sequence.
void AudioProcessor::SendLogMessage(const std::string& message) {
  log_callback_.Run(base::StringPrintf("MSAP::%s [this=0x%" PRIXPTR "]",
                                       message.c_str(),
                                       reinterpret_cast<uintptr_t>(this)));
}

std::optional<AudioParameters> AudioProcessor::ComputeInputFormat(
    const AudioParameters& device_format,
    const AudioProcessingSettings& audio_processing_settings) {
  const ChannelLayout channel_layout = device_format.channel_layout();

  // The audio processor can only handle up to two channels.
  if (channel_layout != CHANNEL_LAYOUT_MONO &&
      channel_layout != CHANNEL_LAYOUT_STEREO &&
      channel_layout != CHANNEL_LAYOUT_DISCRETE) {
    return std::nullopt;
  }

  AudioParameters params(
      device_format.format(), device_format.channel_layout_config(),
      device_format.sample_rate(),
      GetCaptureBufferSize(
          audio_processing_settings.NeedWebrtcAudioProcessing(),
          device_format));
  params.set_effects(device_format.effects());
  if (channel_layout == CHANNEL_LAYOUT_DISCRETE) {
    DCHECK_LE(device_format.channels(), 2);
  }
  DVLOG(1) << params.AsHumanReadableString();
  CHECK(params.IsValid());
  return params;
}

// If WebRTC audio processing is used, the default output format is fixed to the
// native WebRTC processing format in order to avoid rebuffering and resampling.
// If not, then the input format is essentially preserved.
// static
AudioParameters AudioProcessor::GetDefaultOutputFormat(
    const AudioParameters& input_format,
    const AudioProcessingSettings& settings) {
  const bool need_webrtc_audio_processing =
      settings.NeedWebrtcAudioProcessing();
  // TODO(crbug.com/1336055): Investigate why chromecast devices need special
  // logic here.
  const int output_sample_rate =
      need_webrtc_audio_processing
          ?
#if BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
          std::min(media::WebRtcAudioProcessingSampleRateHz(),
                   input_format.sample_rate())
#else
          media::WebRtcAudioProcessingSampleRateHz()
#endif
          : input_format.sample_rate();

  media::ChannelLayoutConfig output_channel_layout_config;
  if (!need_webrtc_audio_processing) {
    output_channel_layout_config = input_format.channel_layout_config();
  } else if (settings.multi_channel_capture_processing) {
    // The number of output channels is equal to the number of input channels.
    // If the media stream audio processor receives stereo input it will
    // output stereo. To reduce computational complexity, APM will not perform
    // full multichannel processing unless any sink requests more than one
    // channel. If the input is multichannel but the sinks are not interested
    // in more than one channel, APM will internally downmix the signal to
    // mono and process it. The processed mono signal will then be upmixed to
    // same number of channels as the input before leaving the media stream
    // audio processor. If a sink later requests stereo, APM will start
    // performing true stereo processing. There will be no need to change the
    // output format.

    output_channel_layout_config = input_format.channel_layout_config();
  } else {
    output_channel_layout_config = ChannelLayoutConfig::Mono();
  }

  // When processing is enabled, the buffer size is dictated by
  // webrtc::AudioProcessing (typically 10 ms). When processing is disabled, we
  // use the same size as the source if it is less than that.
  //
  // TODO(ajm): This conditional buffer size appears to be assuming knowledge of
  // the sink based on the source parameters. PeerConnection sinks seem to want
  // 10 ms chunks regardless, while WebAudio sinks want less, and we're assuming
  // we can identify WebAudio sinks by the input chunk size. Less fragile would
  // be to have the sink actually tell us how much it wants (as in the above
  // todo).
  int output_frames = webrtc::AudioProcessing::GetFrameSize(output_sample_rate);
  if (!need_webrtc_audio_processing &&
      input_format.frames_per_buffer() < output_frames) {
    output_frames = input_format.frames_per_buffer();
  }

  media::AudioParameters output_format = media::AudioParameters(
      input_format.format(), output_channel_layout_config, output_sample_rate,
      output_frames);
  return output_format;
}

}  // namespace media
