// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/simple_sources.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <numbers>
#include <string_view>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"

namespace media {
namespace {
// Opens |wav_filename|, reads it and loads it as a wav file. This function will
// return a null pointer if we can't read the file or if it's malformed. The
// caller takes ownership of the returned data. The size of the data is stored
// in |read_length|.
std::unique_ptr<char[]> ReadWavFile(const base::FilePath& wav_filename,
                                    size_t* read_length) {
  base::File wav_file(wav_filename,
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!wav_file.IsValid()) {
    LOG(ERROR) << "Failed to read " << wav_filename.value()
               << " as input to the fake device."
                  " Try disabling the sandbox with --no-sandbox.";
    return nullptr;
  }

  int64_t wav_file_length = wav_file.GetLength();
  if (wav_file_length < 0) {
    LOG(ERROR) << "Failed to get size of " << wav_filename.value();
    return nullptr;
  }
  if (wav_file_length == 0) {
    LOG(ERROR) << "Input file to fake device is empty: "
               << wav_filename.value();
    return nullptr;
  }

  auto data = std::make_unique<char[]>(wav_file_length);
  int read_bytes = wav_file.Read(0, data.get(), wav_file_length);
  if (read_bytes != wav_file_length) {
    LOG(ERROR) << "Failed to read all bytes of " << wav_filename.value();
    return nullptr;
  }
  *read_length = wav_file_length;
  return data;
}

// These values are based on experiments for local-to-local
// PeerConnection to demonstrate audio/video synchronization.
static const int kBeepDurationMilliseconds = 20;
static const int kBeepFrequency = 400;

// Intervals between two automatic beeps.
static const int kAutomaticBeepIntervalInMs = 500;

// Automatic beep will be triggered every |kAutomaticBeepIntervalInMs| unless
// users explicitly call BeepOnce(), which will disable the automatic beep.
class BeepContext {
 public:
  BeepContext() : beep_once_(false), automatic_beep_(true) {}

  void SetBeepOnce(bool enable) {
    base::AutoLock auto_lock(lock_);
    beep_once_ = enable;

    // Disable the automatic beep if users explicit set |beep_once_| to true.
    if (enable)
      automatic_beep_ = false;
  }

  bool beep_once() const {
    base::AutoLock auto_lock(lock_);
    return beep_once_;
  }

  bool automatic_beep() const {
    base::AutoLock auto_lock(lock_);
    return automatic_beep_;
  }

 private:
  mutable base::Lock lock_;
  bool beep_once_ GUARDED_BY(lock_);
  bool automatic_beep_ GUARDED_BY(lock_);
};

BeepContext* GetBeepContext() {
  static BeepContext* context = new BeepContext();
  return context;
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// SineWaveAudioSource implementation.

SineWaveAudioSource::SineWaveAudioSource(int channels,
                                         double freq,
                                         double sample_freq)
    : channels_(channels), f_(freq / sample_freq) {}

SineWaveAudioSource::~SineWaveAudioSource() = default;

// The implementation could be more efficient if a lookup table is constructed
// but it is efficient enough for our simple needs.
int SineWaveAudioSource::OnMoreData(base::TimeDelta /* delay */,
                                    base::TimeTicks /* delay_timestamp */,
                                    const AudioGlitchInfo& /* glitch_info */,
                                    AudioBus* dest) {
  int max_frames;

  {
    base::AutoLock auto_lock(lock_);
    callbacks_++;

    // The table is filled with s(t) = kint16max*sin(Theta*t),
    // where Theta = 2*PI*fs.
    // We store the discrete time value |t| in a member to ensure that the
    // next pass starts at a correct state.
    max_frames = cap_ > 0 ? std::min(dest->frames(), cap_ - pos_samples_)
                          : dest->frames();
    for (int i = 0; i < max_frames; ++i)
      dest->channel(0)[i] = sin(2.0 * std::numbers::pi * f_ * pos_samples_++);
    for (int i = 1; i < dest->channels(); ++i) {
      memcpy(dest->channel(i), dest->channel(0),
             max_frames * sizeof(*dest->channel(i)));
    }
  }

  if (on_more_data_callback_)
    on_more_data_callback_.Run();

  return max_frames;
}

void SineWaveAudioSource::OnError(ErrorType type) {
  errors_++;
}

void SineWaveAudioSource::CapSamples(int cap) {
  base::AutoLock auto_lock(lock_);
  DCHECK_GT(cap, 0);
  cap_ = cap;
}

void SineWaveAudioSource::Reset() {
  base::AutoLock auto_lock(lock_);
  pos_samples_ = 0;
}

FileSource::FileSource(const AudioParameters& params,
                       const base::FilePath& path_to_wav_file,
                       bool loop)
    : params_(params),
      path_to_wav_file_(path_to_wav_file),
      load_failed_(false),
      looping_(loop) {}

FileSource::~FileSource() = default;

void FileSource::LoadWavFile(const base::FilePath& path_to_wav_file) {
  // Don't try again if we already failed.
  if (load_failed_)
    return;

  // Read the file, and put its data in a scoped_ptr so it gets deleted when
  // this class destructs. This data must be valid for the lifetime of
  // |wav_audio_handler_|.
  size_t length = 0u;
  raw_wav_data_ = ReadWavFile(path_to_wav_file, &length);
  if (!raw_wav_data_) {
    load_failed_ = true;
    return;
  }

  // Attempt to create a handler with this data. If the data is invalid, return.
  wav_audio_handler_ =
      WavAudioHandler::Create(std::string_view(raw_wav_data_.get(), length));
  if (!wav_audio_handler_) {
    LOG(ERROR) << "WAV data could be read but is not valid";
    load_failed_ = true;
    return;
  }

  // Hook us up so we pull in data from the file into the converter. We need to
  // modify the wav file's audio parameters since we'll be reading small slices
  // of it at a time and not the whole thing (like 10 ms at a time).
  AudioParameters file_audio_slice(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(wav_audio_handler_->GetNumChannels()),
      wav_audio_handler_->GetSampleRate(), params_.frames_per_buffer());

  file_audio_converter_ =
      std::make_unique<AudioConverter>(file_audio_slice, params_, false);
  file_audio_converter_->AddInput(this);
}

int FileSource::OnMoreData(base::TimeDelta /* delay */,
                           base::TimeTicks /* delay_timestamp */,
                           const AudioGlitchInfo& /* glitch_info */,
                           AudioBus* dest) {
  // Load the file if we haven't already. This load needs to happen on the
  // audio thread, otherwise we'll run on the UI thread on Mac for instance.
  // This will massively delay the first OnMoreData, but we'll catch up.
  if (!wav_audio_handler_)
    LoadWavFile(path_to_wav_file_);
  if (load_failed_)
    return 0;

  DCHECK(wav_audio_handler_.get());

  if (wav_audio_handler_->AtEnd()) {
    if (looping_)
      Rewind();
    else
      return 0;
  }

  // This pulls data from ProvideInput.
  file_audio_converter_->Convert(dest);
  return dest->frames();
}

void FileSource::Rewind() {
  wav_audio_handler_->Reset();
}

double FileSource::ProvideInput(AudioBus* audio_bus_into_converter,
                                uint32_t frames_delayed,
                                const AudioGlitchInfo&) {
  // Unfilled frames will be zeroed by CopyTo.
  size_t frames_written;
  wav_audio_handler_->CopyTo(audio_bus_into_converter, &frames_written);
  return 1.0;
}

void FileSource::OnError(ErrorType type) {}

BeepingSource::BeepingSource(const AudioParameters& params)
    : buffer_size_(params.GetBytesPerBuffer(kSampleFormatU8)),
      buffer_(new uint8_t[buffer_size_]),
      params_(params),
      last_callback_time_(base::TimeTicks::Now()),
      beep_duration_in_buffers_(kBeepDurationMilliseconds *
                                params.sample_rate() /
                                params.frames_per_buffer() / 1000),
      beep_generated_in_buffers_(0),
      beep_period_in_frames_(params.sample_rate() / kBeepFrequency) {}

BeepingSource::~BeepingSource() = default;

int BeepingSource::OnMoreData(base::TimeDelta /* delay */,
                              base::TimeTicks /* delay_timestamp */,
                              const AudioGlitchInfo& /* glitch_info */,
                              AudioBus* dest) {
  // Accumulate the time from the last beep.
  interval_from_last_beep_ += base::TimeTicks::Now() - last_callback_time_;

  memset(buffer_.get(), 128, buffer_size_);
  bool should_beep = false;
  BeepContext* beep_context = GetBeepContext();
  if (beep_context->automatic_beep()) {
    base::TimeDelta delta = interval_from_last_beep_ -
                            base::Milliseconds(kAutomaticBeepIntervalInMs);
    if (delta.is_positive()) {
      should_beep = true;
      interval_from_last_beep_ = delta;
    }
  } else {
    should_beep = beep_context->beep_once();
    beep_context->SetBeepOnce(false);
  }

  // If this object was instructed to generate a beep or has started to
  // generate a beep sound.
  if (should_beep || beep_generated_in_buffers_) {
    // Compute the number of frames to output high value. Then compute the
    // number of bytes based on channels.
    int high_frames = beep_period_in_frames_ / 2;
    int high_bytes = high_frames * params_.channels();

    // Separate high and low with the same number of bytes to generate a
    // square wave.
    int position = 0;
    while (position + high_bytes <= buffer_size_) {
      // Write high values first.
      memset(buffer_.get() + position, 255, high_bytes);
      // Then leave low values in the buffer with |high_bytes|.
      position += high_bytes * 2;
    }

    ++beep_generated_in_buffers_;
    if (beep_generated_in_buffers_ >= beep_duration_in_buffers_)
      beep_generated_in_buffers_ = 0;
  }

  last_callback_time_ = base::TimeTicks::Now();
  dest->FromInterleaved<UnsignedInt8SampleTypeTraits>(buffer_.get(),
                                                      dest->frames());
  return dest->frames();
}

void BeepingSource::OnError(ErrorType type) {}

void BeepingSource::BeepOnce() {
  GetBeepContext()->SetBeepOnce(true);
}

}  // namespace media
