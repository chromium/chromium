// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WAV_AUDIO_HANDLER_H_
#define MEDIA_AUDIO_WAV_AUDIO_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/time/time.h"
#include "media/audio/audio_handler.h"
#include "media/base/media_export.h"

namespace media {

class AudioBus;

// This class provides the input from wav file format.  See
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
class MEDIA_EXPORT WavAudioHandler : public AudioHandler {
 public:
  // Supported audio format.
  enum class AudioFormat : uint16_t {
    kAudioFormatPCM = 0x0001,
    kAudioFormatFloat = 0x0003,
    kAudioFormatExtensible = 0xfffe
  };

  WavAudioHandler(const WavAudioHandler&) = delete;
  WavAudioHandler& operator=(const WavAudioHandler&) = delete;

  ~WavAudioHandler() override;

  // Create a WavAudioHandler using |wav_data|. If |wav_data| cannot be parsed
  // correctly, the returned scoped_ptr will be null. The underlying memory for
  // wav_data must survive for the lifetime of this class.
  static std::unique_ptr<WavAudioHandler> Create(std::string_view wav_data);

  // AudioHandler:
  bool Initialize() override;
  int GetNumChannels() const override;
  int GetSampleRate() const override;
  base::TimeDelta GetDuration() const override;
  bool AtEnd() const override;
  bool CopyTo(AudioBus* bus, size_t* frames_written) override;
  void Reset() override;

  // Accessors.
  std::string_view data() const { return audio_data_; }
  AudioFormat audio_format() const { return audio_format_; }

  int total_frames_for_testing() const {
    return static_cast<int>(total_frames_);
  }
  int bits_per_sample_for_testing() const {
    return static_cast<int>(bits_per_sample_);
  }

 private:
  // Note: It is preferred to pass |audio_data| by value here.
  WavAudioHandler(std::string_view audio_data,
                  uint16_t num_channels,
                  uint32_t sample_rate,
                  uint16_t bits_per_sample,
                  AudioFormat audio_format);

  // Data part of the |wav_data_|.
  const std::string_view audio_data_;
  const uint16_t num_channels_;
  const uint32_t sample_rate_;
  const uint16_t bits_per_sample_;
  const AudioFormat audio_format_;
  uint32_t total_frames_;

  size_t cursor_ = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WAV_AUDIO_HANDLER_H_
