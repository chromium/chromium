// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_auhal_mac.h"

#include <CoreServices/CoreServices.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/mac/core_audio_util_mac.h"
#include "media/base/audio_pull_fifo.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

namespace {

// Mapping from Chrome's channel layout to CoreAudio layout. This must match the
// layout of the Channels enum in |channel_layout.h|
static const AudioChannelLabel kCoreAudioChannelMapping[] = {
    kAudioChannelLabel_Left,
    kAudioChannelLabel_Right,
    kAudioChannelLabel_Center,
    kAudioChannelLabel_LFEScreen,
    kAudioChannelLabel_LeftSurround,
    kAudioChannelLabel_RightSurround,
    kAudioChannelLabel_LeftCenter,
    kAudioChannelLabel_RightCenter,
    kAudioChannelLabel_CenterSurround,
    kAudioChannelLabel_LeftSurroundDirect,
    kAudioChannelLabel_RightSurroundDirect,
};
static_assert(0 == LEFT && 1 == RIGHT && 2 == CENTER && 3 == LFE &&
                  4 == BACK_LEFT &&
                  5 == BACK_RIGHT &&
                  6 == LEFT_OF_CENTER &&
                  7 == RIGHT_OF_CENTER &&
                  8 == BACK_CENTER &&
                  9 == SIDE_LEFT &&
                  10 == SIDE_RIGHT &&
                  10 == CHANNELS_MAX,
              "Channel positions must match CoreAudio channel order.");

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

// Converts |channel_layout| into CoreAudio format and sets up the AUHAL with
// our layout information so it knows how to remap the channels.
void SetAudioChannelLayout(int channels,
                           ChannelLayout channel_layout,
                           AudioUnit audio_unit) {
  DCHECK(audio_unit);
  DCHECK_GT(channels, 0);
  DCHECK_GT(channel_layout, CHANNEL_LAYOUT_UNSUPPORTED);

  // AudioChannelLayout is structure ending in a variable length array, so we
  // can't directly allocate one. Instead compute the size and and allocate one
  // inside of a byte array.
  //
  // Code modeled after example from Apple documentation here:
  // https://developer.apple.com/library/content/qa/qa1627/_index.html
  const size_t layout_size =
      offsetof(AudioChannelLayout, mChannelDescriptions[channels]);
  std::unique_ptr<uint8_t[]> layout_storage(new uint8_t[layout_size]);
  memset(layout_storage.get(), 0, layout_size);
  AudioChannelLayout* coreaudio_layout =
      reinterpret_cast<AudioChannelLayout*>(layout_storage.get());

  coreaudio_layout->mNumberChannelDescriptions = channels;
  coreaudio_layout->mChannelLayoutTag =
      kAudioChannelLayoutTag_UseChannelDescriptions;
  AudioChannelDescription* descriptions =
      coreaudio_layout->mChannelDescriptions;

  if (channel_layout == CHANNEL_LAYOUT_DISCRETE) {
    // For the discrete case just assume common input mappings; once we run out
    // of known channels mark them as unknown.
    for (int ch = 0; ch < channels; ++ch) {
      descriptions[ch].mChannelLabel = ch > CHANNELS_MAX
                                           ? kAudioChannelLabel_Unknown
                                           : kCoreAudioChannelMapping[ch];
      descriptions[ch].mChannelFlags = kAudioChannelFlags_AllOff;
    }
  } else if (channel_layout == CHANNEL_LAYOUT_MONO) {
    // CoreAudio has a special label for mono.
    DCHECK_EQ(channels, 1);
    descriptions[0].mChannelLabel = kAudioChannelLabel_Mono;
    descriptions[0].mChannelFlags = kAudioChannelFlags_AllOff;
  } else {
    for (int ch = 0; ch <= CHANNELS_MAX; ++ch) {
      const int order = ChannelOrder(channel_layout, static_cast<Channels>(ch));
      if (order == -1)
        continue;
      descriptions[order].mChannelLabel = kCoreAudioChannelMapping[ch];
      descriptions[order].mChannelFlags = kAudioChannelFlags_AllOff;
    }
  }

  OSStatus result = AudioUnitSetProperty(
      audio_unit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input,
      AUElement::OUTPUT, coreaudio_layout, layout_size);
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

AUHALStream::AUHALStream(AUHALStreamClient* client,
                         const AudioParameters& params,
                         AudioDeviceID device,
                         const AudioManager::LogCallback& log_callback)
    : client_(client),
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
  DCHECK(client_);
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
    hardware_latency_ = core_audio_mac::GetHardwareLatency(
        audio_unit_->audio_unit(), device_, kAudioObjectPropertyScopeOutput,
        params_.sample_rate());
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
  client_->ReleaseOutputStreamUsingRealDevice(this, device_);
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
  base::TimeDelta defer_start = client_->GetDeferStreamStartTimeout();
  if (!defer_start.is_zero()) {
    // Use a cancellable closure so that if Stop() is called before Start()
    // actually runs, we can cancel the pending start.
    deferred_start_cb_.Reset(
        base::BindOnce(&AUHALStream::Start, base::Unretained(this), callback));
    client_->GetTaskRunner()->PostDelayedTask(
        FROM_HERE, deferred_start_cb_.callback(), defer_start);
    return;
  }
#endif

  stopped_ = false;

  {
    base::AutoLock al(lock_);
    DCHECK(!audio_fifo_);
    source_ = callback;
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
  TRACE_EVENT2("audio", "AUHALStream::Render", "input buffer size",
               params_.frames_per_buffer(), "output buffer size",
               number_of_frames);

  base::AutoLock al(lock_);

  // There's no documentation on what we should return here, but if we're here
  // something is wrong so just return an AudioUnit error that looks reasonable.
  if (!source_)
    return kAudioUnitErr_Uninitialized;

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

  last_number_of_frames_ = number_of_frames;

  return noErr;
}

void AUHALStream::ProvideInput(int frame_delay, AudioBus* dest) {
  TRACE_EVENT1("audio", "AUHALStream::ProvideInput", "frames", dest->frames());
  lock_.AssertAcquired();
  DCHECK(source_);

  const base::TimeTicks playout_time =
      current_playout_time_ +
      AudioTimestampHelper::FramesToTime(frame_delay, params_.sample_rate());
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta delay = playout_time - now;

  // Supply the input data and render the output data.
  source_->OnMoreData(delay, now, glitch_info_accumulator_.GetAndReset(), dest);
  dest->Scale(volume_);
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
  // A platform bug has been observed where the platform sometimes reports that
  // the next frames will be output at an invalid time or a time in the past.
  // Because the target playout time cannot be invalid or in the past, return
  // "now" in these cases.
  if ((output_time_stamp->mFlags & kAudioTimeStampHostTimeValid) == 0)
    return base::TimeTicks::Now();

  return std::max(base::TimeTicks::FromMachAbsoluteTime(
                      output_time_stamp->mHostTime),
                  base::TimeTicks::Now()) +
         hardware_latency_;
}

void AUHALStream::UpdatePlayoutTimestamp(const AudioTimeStamp* timestamp) {
  lock_.AssertAcquired();

  if ((timestamp->mFlags & kAudioTimeStampSampleTimeValid) == 0)
    return;

  if (last_sample_time_) {
    DCHECK_NE(0U, last_number_of_frames_);
    UInt32 sample_time_diff =
        static_cast<UInt32>(timestamp->mSampleTime - last_sample_time_);
    DCHECK_GE(sample_time_diff, last_number_of_frames_);
    UInt32 lost_frames = sample_time_diff - last_number_of_frames_;
    base::TimeDelta lost_audio_duration =
        AudioTimestampHelper::FramesToTime(lost_frames, params_.sample_rate());
    glitch_reporter_.UpdateStats(lost_audio_duration);
    if (!lost_audio_duration.is_zero()) {
      glitch_info_accumulator_.Add(
          AudioGlitchInfo{.duration = lost_audio_duration, .count = 1});
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

  if (!client_->MaybeChangeBufferSize(device_, local_audio_unit->audio_unit(),
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
