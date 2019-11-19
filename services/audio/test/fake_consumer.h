// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_FAKE_CONSUMER_H_
#define SERVICES_AUDIO_TEST_FAKE_CONSUMER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"

namespace media {
class AudioBus;
}

namespace audio {

// Consumes and records the audio signal. Then, test procedures may use the
// utility methods to analyze the recording.
class FakeConsumer {
 public:
  FakeConsumer(int channels, int sample_rate);

  ~FakeConsumer();

  // Returns the total number of frames recorded (the end position of the
  // recording).
  int GetRecordedFrameCount() const;

  // Throws away all samples recorded so far.
  void Clear();

  // Appends the audio signal in |bus| to the recording.
  void Consume(const media::AudioBus& bus);

  // Returns true if all (or a part) of the recording in |channel| is flat
  // (i.e., not oscillating, and effectively silent).
  bool IsSilent(int channel) const;
  bool IsSilentInRange(int channel, int begin_frame, int end_frame) const;

  // Returns the position at which silence in the recording ends, at or after
  // |begin_frame|. If the entire recording after the given position is silent,
  // this will return GetRecordedFrameCount().
  int FindEndOfSilence(int channel, int begin_frame) const;

  // Returns the amplitude of the given |frequency| in the given |channel| just
  // before the given |end_frame| position.
  double ComputeAmplitudeAt(int channel, double frequency, int end_frame) const;

  // Saves the recorded content to a WAV-format file, overwriting it if it
  // exists.
  void SaveToFile(const base::FilePath& path) const;

 private:
  const int sample_rate_;
  std::vector<std::vector<float>> recorded_channel_data_;

  DISALLOW_COPY_AND_ASSIGN(FakeConsumer);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_FAKE_CONSUMER_H_
