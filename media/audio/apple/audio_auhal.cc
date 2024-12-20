// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/apple/audio_auhal.h"

#include <CoreServices/CoreServices.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "base/apple/osstatus_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/mac/channel_layout_util_mac.h"

#if BUILDFLAG(IS_MAC)
#include "media/audio/mac/core_audio_util_mac.h"
#endif

namespace media {

namespace {

void WrapBufferList(AudioBufferList* buffer_list, AudioBus* bus, int frames) {
  const int channels = bus->channels();
  const int buffer_list_channels = buffer_list->mNumberBuffers;
  CHECK_EQ(channels, buffer_list_channels);

  // Copy pointers from AudioBufferList.
  for (int i = 0; i < channels; ++i)
    bus->SetChannelData(i, static_cast<float*>(buffer_list->mBuffers[i].mData));

  // Finally set the actual length.
  bus->set_frames(frames);
}

// Sets the stream format on the AUHAL to PCM Float32 non-interleaved for the
// given number of channels on the given scope and element. The created stream
// description will be stored in |desc|.
bool SetStreamFormat(int channels,
                     int sample_rate,
                     AudioUnit audio_unit,
                     AudioStreamBasicDescription* format) {
  format->mSampleRate = sample_rate;
  format->mFormatID = kAudioFormatLinearPCM;
  format->mFormatFlags = AudioFormatFlags{kAudioFormatFlagsNativeFloatPacked} |
                         kLinearPCMFormatFlagIsNonInterleaved;
  format->mBytesPerPacket = sizeof(Float32);
  format->mFramesPerPacket = 1;
  format->mBytesPerFrame = sizeof(Float32);
  format->mChannelsPerFrame = channels;
  format->mBitsPerChannel = 32;
  format->mReserved = 0;

  // Set stream formats. See Apple's tech note for details on the peculiar way
  // that inputs and outputs are handled in the AUHAL concerning scope and bus
  // (element) numbers:
  // http://developer.apple.com/library/mac/#technotes/tn2091/_index.html
  return AudioUnitSetProperty(audio_unit, kAudioUnitProperty_StreamFormat,
                              kAudioUnitScope_Input, AUElement::OUTPUT, format,
                              sizeof(*format)) == noErr;
}

// Try map Rls & Rrs channel to Ls & Rs if necessary.
//
// Most of the configurable audio channel layout in Audio MIDI uses Side Left
// (Ls) and Side Right (Rs) as the default surround channel, while some of the
// WAV/FLAC/AAC audios uses Back Left (Rls) and Back Right (Rrs) as default
// surround channel. If we unconditionally treat Rls and Rrs as it is, and if
// Audio MIDI device has no Rls and Rrs channels (only `7.1 Surround` and
// `7.1.4` configuration has Rls & Rrs channel for now), then these two
// channels will be silent. QuickTime is doing something correct, so we
// can do something similar here, the overall logic will be:
//
// 1. If Audio has no Rls & Rrs channels -> Do nothing.
// 2. If Audio has Rls & Rrs channels, and has Ls & Rs channels -> Do nothing.
// 3. If Audio has Rls & Rrs channels, and has no Ls & Rs channels, device has
//    Rls & Rrs channels -> Do nothing.
// 4. If Audio has Rls & Rrs channels, and has no Ls & Rs channels, device has
//    no Rls & Rrs channels -> Map Rls to Ls, Rrs to Rs.
void MaybeMapRearSurroundChannelToSurroundChannel(
    const AudioUnit& audio_unit,
    AudioChannelLayout* audio_layout) {
  bool maybe_need_mapping = false;
  for (UInt32 i = 0; i < audio_layout->mNumberChannelDescriptions; i++) {
    AudioChannelLabel label =
        audio_layout->mChannelDescriptions[i].mChannelLabel;
    // If audio already has Ls or Rs channel, skip.
    if (label == kAudioChannelLabel_LeftSurround ||
        label == kAudioChannelLabel_RightSurround) {
      return;
    }
    if (label == kAudioChannelLabel_RearSurroundLeft ||
        label == kAudioChannelLabel_RearSurroundRight) {
      maybe_need_mapping = true;
    }
  }

  // If audio has no Rls or Rrs channel, skip.
  if (!maybe_need_mapping) {
    return;
  }

  auto scoped_device_layout =
      AudioManagerApple::GetOutputDeviceChannelLayout(audio_unit);
  if (!scoped_device_layout) {
    return;
  }
  AudioChannelLayout* device_layout = scoped_device_layout->layout();

  // If device has Rls or Rrs channel, skip. Since we only do mapping when
  // Rls or Rrs channel do not exist.
  for (UInt32 i = 0; i < device_layout->mNumberChannelDescriptions; ++i) {
    AudioChannelLabel label =
        device_layout->mChannelDescriptions[i].mChannelLabel;
    if (label == kAudioChannelLabel_RearSurroundLeft ||
        label == kAudioChannelLabel_RearSurroundRight) {
      return;
    }
  }

  // Map Rls to Ls, Rrs to Rs.
  for (UInt32 i = 0; i < audio_layout->mNumberChannelDescriptions; i++) {
    AudioChannelLabel label =
        audio_layout->mChannelDescriptions[i].mChannelLabel;
    if (label == kAudioChannelLabel_RearSurroundLeft) {
      audio_layout->mChannelDescriptions[i].mChannelLabel =
          kAudioChannelLabel_LeftSurround;
    } else if (label == kAudioChannelLabel_RearSurroundRight) {
      audio_layout->mChannelDescriptions[i].mChannelLabel =
          kAudioChannelLabel_RightSurround;
    }
  }
}

// Converts |channel_layout| into CoreAudio format and sets up the AUHAL with
// our layout information so it knows how to remap the channels.
void SetAudioChannelLayout(int channels,
                           ChannelLayout channel_layout,
                           AudioUnit audio_unit) {
  DCHECK(audio_unit);
  DCHECK_GT(channels, 0);
  DCHECK_GT(channel_layout, CHANNEL_LAYOUT_UNSUPPORTED);

  auto coreaudio_layout =
      ChannelLayoutToAudioChannelLayout(channel_layout, channels);

  MaybeMapRearSurroundChannelToSurroundChannel(audio_unit,
                                               coreaudio_layout->layout());

  OSStatus result = AudioUnitSetProperty(
      audio_unit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input,
      AUElement::OUTPUT, coreaudio_layout->layout(),
      coreaudio_layout->layout_size());
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "Failed to set audio channel layout. Using default layout.";
  }
}

void ReportFramesRequestedUma(int number_of_frames_requested) {
  // A value of 0 indicates that we got the buffer size we asked for.
  base::UmaHistogramCounts1M("Media.Audio.Render.FramesRequested",
                             number_of_frames_requested);
}

}  // namespace

AUHALStream::AUHALStream(AudioManagerApple* manager,
                         const AudioParameters& params,
                         AudioDeviceID device,
                         const AudioManager::LogCallback& log_callback)
    : manager_(manager),
      params_(params),
      source_(nullptr),
      device_(device),
      volume_(1),
      stopped_(true),
      last_sample_time_(0.0),
      last_number_of_frames_(0),
      glitch_reporter_(SystemGlitchReporter::StreamType::kRender),
      log_callback_(log_callback) {
  // We must have a manager.
  DVLOG(1) << __FUNCTION__ << " this " << this << " params "
           << params.AsHumanReadableString();
  DCHECK(manager_);
  DCHECK(params_.IsValid());
#if BUILDFLAG(IS_MAC)
  DCHECK_NE(device, kAudioObjectUnknown);
#endif
}

AUHALStream::~AUHALStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!audio_unit_);
}

bool AUHALStream::Open() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!output_bus_);
  DCHECK(!audio_unit_);

  // The output bus will wrap the AudioBufferList given to us in
  // the Render() callback.
  output_bus_ = AudioBus::CreateWrapper(params_.channels());

  bool configured = ConfigureAUHAL();
  if (configured) {
    DCHECK(audio_unit_);
    DCHECK(audio_unit_->is_valid());
#if BUILDFLAG(IS_MAC)
    hardware_latency_ = core_audio_mac::GetHardwareLatency(
        audio_unit_->audio_unit(), device_, kAudioObjectPropertyScopeOutput,
        params_.sample_rate(), /*is_input=*/false);
#else
    // TODO(crbug.com/40255660): Implement me.
    hardware_latency_ = base::TimeDelta();
#endif
  }

  DVLOG(1) << __FUNCTION__ << " this " << this << " received hardware latency "
           << hardware_latency_;
  return configured;
}

void AUHALStream::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __FUNCTION__ << " this " << this;

  if (audio_unit_) {
    Stop();

    // Clear the render callback to try and prevent any callbacks from coming
    // in after we've called stop. https://crbug.com/737527.
    AURenderCallbackStruct callback = {0};
    auto result = AudioUnitSetProperty(
        audio_unit_->audio_unit(), kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Input, AUElement::OUTPUT, &callback, sizeof(callback));
    OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
        << "Failed to clear input callback.";
  }

  audio_unit_.reset();
  // Inform the audio manager that we have been closed. This will cause our
  // destruction. Also include the device ID as a signal to the audio manager
  // that it should try to increase the native I/O buffer size after the stream
  // has been closed.
  manager_->ReleaseOutputStream(this);
}

void AUHALStream::Start(AudioSourceCallback* callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(callback);
  if (!audio_unit_) {
    DLOG(ERROR) << "Open() has not been called successfully";
    return;
  }

  if (!stopped_) {
    base::AutoLock al(lock_);
    CHECK_EQ(source_, callback);
    return;
  }

  DVLOG(1) << __FUNCTION__ << " this " << this;

#if BUILDFLAG(IS_MAC)
  // Check if we should defer Start() for http://crbug.com/160920.
  base::TimeDelta defer_start = manager_->GetDeferStreamStartTimeout();
  if (!defer_start.is_zero()) {
    // Use a cancellable closure so that if Stop() is called before Start()
    // actually runs, we can cancel the pending start.
    deferred_start_cb_.Reset(
        base::BindOnce(&AUHALStream::Start, base::Unretained(this), callback));
    manager_->GetTaskRunner()->PostDelayedTask(
        FROM_HERE, deferred_start_cb_.callback(), defer_start);
    return;
  }
#endif

  stopped_ = false;

  {
    base::AutoLock al(lock_);
    DCHECK(!audio_fifo_);
    source_ = callback;

#if BUILDFLAG(IS_MAC)
    peak_detector_ = std::make_unique<AmplitudePeakDetector>(
        base::BindRepeating(&AudioManagerApple::StopAmplitudePeakTrace,
                            base::Unretained(manager_)));
#endif
  }

  OSStatus result = AudioOutputUnitStart(audio_unit_->audio_unit());
  if (result == noErr)
    return;

  Stop();
  OSSTATUS_DLOG(ERROR, result) << "AudioOutputUnitStart() failed.";
  callback->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void AUHALStream::Flush() {}

void AUHALStream::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  deferred_start_cb_.Cancel();
  if (stopped_)
    return;

  DVLOG(1) << __FUNCTION__ << " this " << this;

  OSStatus result = AudioOutputUnitStop(audio_unit_->audio_unit());
  OSSTATUS_DLOG_IF(ERROR, result != noErr, result)
      << "AudioOutputUnitStop() failed.";

  {
    base::AutoLock al(lock_);
    if (result != noErr)
      source_->OnError(AudioSourceCallback::ErrorType::kUnknown);
    source_ = nullptr;

    if (last_sample_time_) {  // Report stats if the stream has been active.
      if (!audio_fifo_)  // Unexpected buffer size has never been requested.
        ReportFramesRequestedUma(0);

      SystemGlitchReporter::Stats stats =
          glitch_reporter_.GetLongTermStatsAndReset();

      std::string log_message = base::StringPrintf(
          "AU out: (num_glitches_detected=[%d], cumulative_audio_lost=[%llu "
          "ms], "
          "largest_glitch=[%llu ms])",
          stats.glitches_detected, stats.total_glitch_duration.InMilliseconds(),
          stats.largest_glitch_duration.InMilliseconds());

      if (!log_callback_.is_null())
        log_callback_.Run(log_message);
      if (stats.glitches_detected > 0) {
        DLOG(WARNING) << log_message;
      }
    }

    last_sample_time_ = 0;
    last_number_of_frames_ = 0;
    audio_fifo_.reset();

#if BUILDFLAG(IS_MAC)
    peak_detector_.reset();
#endif
  }

  stopped_ = true;
}

void AUHALStream::SetVolume(double volume) {
  volume_ = static_cast<float>(volume);
}

void AUHALStream::GetVolume(double* volume) {
  *volume = volume_;
}

// Pulls on our provider to get rendered audio stream.
// Note to future hackers of this function: Do not add locks which can
// be contended in the middle of stream processing here (starting and stopping
// the stream are ok) because this is running on a real-time thread.
OSStatus AUHALStream::Render(AudioUnitRenderActionFlags* flags,
                             const AudioTimeStamp* output_time_stamp,
                             UInt32 bus_number,
                             UInt32 number_of_frames,
                             AudioBufferList* data) {
  base::AutoLock al(lock_);

  // There's no documentation on what we should return here, but if we're here
  // something is wrong so just return an AudioUnit error that looks reasonable.
  if (!source_)
    return kAudioUnitErr_Uninitialized;

  TRACE_EVENT_BEGIN(
      "audio", "AUHALStream::Render", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_mac_auhal_stream();
        data->set_input_buffer_size(params_.frames_per_buffer());
        data->set_output_buffer_size(number_of_frames);
        data->set_sample_rate(params_.sample_rate());
      });

  UpdatePlayoutTimestamp(output_time_stamp);

  // If the stream parameters change for any reason, we need to insert a FIFO
  // since the OnMoreData() pipeline can't handle frame size changes.
  if (number_of_frames != static_cast<UInt32>(params_.frames_per_buffer())) {
    // Create a FIFO on the fly to handle any discrepancies in callback rates.
    if (!audio_fifo_) {
      DVLOG(1) << __FUNCTION__ << " this " << this
               << "Audio frame size changed from "
               << params_.frames_per_buffer() << " to " << number_of_frames
               << " adding FIFO to compensate.";
      audio_fifo_ = std::make_unique<AudioPullFifo>(
          params_.channels(), params_.frames_per_buffer(),
          base::BindRepeating(&AUHALStream::ProvideInput,
                              base::Unretained(this)));
      // Report it only once the first time the change happens.
      ReportFramesRequestedUma(number_of_frames);
    } else if (last_number_of_frames_ != number_of_frames) {
      DVLOG(3) << __FUNCTION__ << " this " << this
               << "Audio frame size changed from " << last_number_of_frames_
               << " to " << number_of_frames << " FIFO already exists.";
    }
  }

  // Make |output_bus_| wrap the output AudioBufferList.
  WrapBufferList(data, output_bus_.get(), number_of_frames);

  current_playout_time_ = GetPlayoutTime(output_time_stamp);

  if (audio_fifo_)
    audio_fifo_->Consume(output_bus_.get(), output_bus_->frames());
  else
    ProvideInput(0, output_bus_.get());

#if BUILDFLAG(IS_MAC)
  peak_detector_->FindPeak(output_bus_.get());
#endif

  last_number_of_frames_ = number_of_frames;

  TRACE_EVENT_END("audio", [&](perfetto::EventContext ctx) {
    auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
    auto* data = event->set_mac_auhal_stream();
    data->set_os_request_playout_timeticks_us(
        (current_playout_time_ - base::TimeTicks()).InMicroseconds());
  });
  return noErr;
}

void AUHALStream::ProvideInput(int frame_delay, AudioBus* dest) {
  TRACE_EVENT_BEGIN(
      "audio", "AUHALStream::ProvideInput", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_mac_auhal_stream();
        data->set_source_request_frames(dest->frames());
      });

  lock_.AssertAcquired();
  DCHECK(source_);

  const base::TimeTicks playout_time =
      current_playout_time_ +
      AudioTimestampHelper::FramesToTime(frame_delay, params_.sample_rate());
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta delay = playout_time - now;

  TRACE_EVENT_INSTANT(
      "audio", "AUHALStream delay", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_mac_auhal_stream();
        data->set_source_request_playout_delay_us(delay.InMicroseconds());
      });

  UMA_HISTOGRAM_COUNTS_1000("Media.Audio.Render.SystemDelay",
                            delay.InMilliseconds());
  // Supply the input data and render the output data.
  source_->OnMoreData(BoundedDelay(delay), now,
                      glitch_info_accumulator_.GetAndReset(), dest);
  dest->Scale(volume_);
  TRACE_EVENT_END("audio", [&](perfetto::EventContext ctx) {
    auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
    auto* data = event->set_mac_auhal_stream();
    data->set_source_request_playout_timeticks_us(
        (playout_time - base::TimeTicks()).InMicroseconds());
    data->set_source_request_current_timeticks_us(
        (now - base::TimeTicks()).InMicroseconds());
  });
}

// AUHAL callback.
OSStatus AUHALStream::InputProc(void* user_data,
                                AudioUnitRenderActionFlags* flags,
                                const AudioTimeStamp* output_time_stamp,
                                UInt32 bus_number,
                                UInt32 number_of_frames,
                                AudioBufferList* io_data) {
  // Dispatch to our class method.
  AUHALStream* audio_output = static_cast<AUHALStream*>(user_data);
  if (!audio_output)
    return -1;

  return audio_output->Render(flags, output_time_stamp, bus_number,
                              number_of_frames, io_data);
}

base::TimeTicks AUHALStream::GetPlayoutTime(
    const AudioTimeStamp* output_time_stamp) {
  TRACE_EVENT_BEGIN(
      TRACE_DISABLED_BY_DEFAULT("audio"), "AUHALStream::GetPlayoutTime",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_mac_auhal_stream();
        data->set_hardware_latency_us(hardware_latency_.InMicroseconds());
      });
  // A platform bug has been observed where the platform sometimes reports that
  // the next frames will be output at an invalid time or a time in the past.
  // Because the target playout time cannot be invalid or in the past, return
  // "now" in these cases.
  if ((output_time_stamp->mFlags & kAudioTimeStampHostTimeValid) == 0) {
    TRACE_EVENT_END(
        TRACE_DISABLED_BY_DEFAULT("audio"), [&](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_mac_auhal_stream();
          data->set_audiotimestamp_host_time_valid(false);
        });
    return base::TimeTicks::Now();
  }

  base::TimeTicks mach_time =
      base::TimeTicks::FromMachAbsoluteTime(output_time_stamp->mHostTime);
  TRACE_EVENT_END(
      TRACE_DISABLED_BY_DEFAULT("audio"), [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_mac_auhal_stream();
        data->set_audiotimestamp_host_time_valid(true);
        data->set_audiotimestamp_mach_timeticks_us(
            (mach_time - base::TimeTicks()).InMicroseconds());
      });
  return std::max(mach_time, base::TimeTicks::Now()) + hardware_latency_;
}

void AUHALStream::UpdatePlayoutTimestamp(const AudioTimeStamp* timestamp) {
  lock_.AssertAcquired();

  if ((timestamp->mFlags & kAudioTimeStampSampleTimeValid) == 0)
    return;

  // Compiler will complain that we pass a lock-guarded variable into the lambda
  // otherwise.
  Float64 lock_free_last_sample_time_ = last_sample_time_;
  TRACE_EVENT(
      "audio", "AUHALStream::UpdatePlayoutTimestamp",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_mac_auhal_stream();
        data->set_audiotimestamp_sample_time_frames(timestamp->mSampleTime);
        data->set_audiotimestamp_last_sample_time_frames(
            lock_free_last_sample_time_);
      });

  // if mSampleTime jumps backwards, do not look for glitches.
  if (last_sample_time_ && last_sample_time_ <= timestamp->mSampleTime) {
    DCHECK_NE(0U, last_number_of_frames_);
    UInt32 sample_time_diff =
        static_cast<UInt32>(timestamp->mSampleTime - last_sample_time_);
    DCHECK_GE(sample_time_diff, last_number_of_frames_);
    UInt32 lost_frames = sample_time_diff - last_number_of_frames_;
    base::TimeDelta lost_audio_duration =
        AudioTimestampHelper::FramesToTime(lost_frames, params_.sample_rate());
    TRACE_EVENT_INSTANT(
        "audio", "AUHALStream lost_audio_duration",
        [&](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_mac_auhal_stream();
          data->set_lost_audio_duration_us(
              lost_audio_duration.InMicroseconds());
        });
    glitch_reporter_.UpdateStats(lost_audio_duration);
    if (!lost_audio_duration.is_zero()) {
      glitch_info_accumulator_.Add(AudioGlitchInfo::SingleBoundedSystemGlitch(
          lost_audio_duration, AudioGlitchInfo::Direction::kRender));
    }
  }

  // Store the last sample time for use next time we get called back.
  last_sample_time_ = timestamp->mSampleTime;
}

bool AUHALStream::ConfigureAUHAL() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<ScopedAudioUnit> local_audio_unit(
      new ScopedAudioUnit(device_, AUElement::OUTPUT));
  if (!local_audio_unit->is_valid())
    return false;

  if (!SetStreamFormat(params_.channels(), params_.sample_rate(),
                       local_audio_unit->audio_unit(), &output_format_)) {
    return false;
  }

  if (!manager_->MaybeChangeBufferSize(device_, local_audio_unit->audio_unit(),
                                       0, params_.frames_per_buffer())) {
    return false;
  }

  // Setup callback.
  AURenderCallbackStruct callback;
  callback.inputProc = InputProc;
  callback.inputProcRefCon = this;
  OSStatus result = AudioUnitSetProperty(
      local_audio_unit->audio_unit(), kAudioUnitProperty_SetRenderCallback,
      kAudioUnitScope_Input, AUElement::OUTPUT, &callback, sizeof(callback));
  if (result != noErr)
    return false;

  SetAudioChannelLayout(params_.channels(), params_.channel_layout(),
                        local_audio_unit->audio_unit());

  result = AudioUnitInitialize(local_audio_unit->audio_unit());
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioUnitInitialize() failed.";
    return false;
  }

  audio_unit_ = std::move(local_audio_unit);
  return true;
}

}  // namespace media
