// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SIMPLE_SOURCES_H_
#define MEDIA_AUDIO_SIMPLE_SOURCES_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_converter.h"
#include "media/base/seekable_buffer.h"

namespace media {

class WavAudioHandler;

// An audio source that produces a pure sinusoidal tone.
class MEDIA_EXPORT SineWaveAudioSource
    : public AudioOutputStream::AudioSourceCallback {
 public:
  // |channels| is the number of audio channels, |freq| is the frequency in
  // hertz and it has to be less than half of the sampling frequency
  // |sample_freq| or else you will get aliasing.
  SineWaveAudioSource(int channels, double freq, double sample_freq);
  ~SineWaveAudioSource() override;

  // Return up to |cap| samples of data via OnMoreData().  Use Reset() to
  // allow more data to be served.
  void CapSamples(int cap);
  void Reset();

  // Sets a callback to be called in OnMoreData(). The callback should be set
  // only once before SineWaveAudioSource is passed to AudioOutputStream. It
  // will be called on the same thread on which the stream calls OnMoreData(),
  // which is usually different from the thread where the source is created.
  void set_on_more_data_callback(base::RepeatingClosure on_more_data_callback) {
    on_more_data_callback_ = on_more_data_callback;
  }

  // Implementation of AudioSourceCallback.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks timestamp,
                 const AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override;
  void OnError(ErrorType type) override;

  // The number of OnMoreData() and OnError() calls respectively.
  int callbacks() {
    base::AutoLock auto_lock(lock_);
    return callbacks_;
  }
  int pos_samples() {
    base::AutoLock auto_lock(lock_);
    return pos_samples_;
  }
  int errors() { return errors_; }

 protected:
  const int channels_;
  const double f_;

  base::RepeatingClosure on_more_data_callback_;

  base::Lock lock_;
  int pos_samples_ GUARDED_BY(lock_) = 0;
  int cap_ GUARDED_BY(lock_) = 0;
  int callbacks_ GUARDED_BY(lock_) = 0;
  int errors_ = 0;
};

class MEDIA_EXPORT FileSource : public AudioOutputStream::AudioSourceCallback,
                                public AudioConverter::InputCallback {
 public:
  FileSource(const AudioParameters& params,
             const base::FilePath& path_to_wav_file,
             bool loop);
  ~FileSource() override;

  // Implementation of AudioSourceCallback.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override;
  void OnError(ErrorType type) override;

 private:
  AudioParameters params_;
  base::FilePath path_to_wav_file_;

  // The WAV data at |path_to_wav_file_| is read into memory and kept here.
  // This memory needs to survive for the lifetime of |wav_audio_handler_|,
  // so declare it first. Do not access this member directly.
  std::unique_ptr<char[]> raw_wav_data_;

  std::unique_ptr<WavAudioHandler> wav_audio_handler_;
  std::unique_ptr<AudioConverter> file_audio_converter_;
  bool load_failed_;
  bool looping_;

  // Provides audio data from wav_audio_handler_ into the file audio converter.
  double ProvideInput(AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const AudioGlitchInfo& glitch_info) override;

  // Loads the wav file on the first OnMoreData invocation.
  void LoadWavFile(const base::FilePath& path_to_wav_file);

  // Rewinds the player to the start of the loaded wav file.
  void Rewind();
};

class BeepingSource : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit BeepingSource(const AudioParameters& params);
  ~BeepingSource() override;

  // Implementation of AudioSourceCallback.
  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 const AudioGlitchInfo& glitch_info,
                 AudioBus* dest) override;
  void OnError(ErrorType type) override;

  static void BeepOnce();

 private:
  int buffer_size_;
  std::unique_ptr<uint8_t[]> buffer_;
  AudioParameters params_;
  base::TimeTicks last_callback_time_;
  base::TimeDelta interval_from_last_beep_;
  int beep_duration_in_buffers_;
  int beep_generated_in_buffers_;
  int beep_period_in_frames_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_SIMPLE_SOURCES_H_
