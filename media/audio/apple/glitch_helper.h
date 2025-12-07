// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_APPLE_GLITCH_HELPER_H_
#define MEDIA_AUDIO_APPLE_GLITCH_HELPER_H_

#include <AudioUnit/AudioUnit.h>

#include <optional>

#include "media/audio/system_glitch_reporter.h"
#include "media/base/audio_glitch_info.h"

namespace media {

class GlitchHelper {
 public:
  GlitchHelper(int sample_rate, AudioGlitchInfo::Direction direction);

  // Detects and update glitch_reporter and glitch_accumulator accordingly.
  // This function should be called at every callback from the OS.
  // `timestamp` is the time stamp we got in the current callback.
  // `frames` is the number of audio frames that we got in the current
  // callback.
  void OnFramesReceived(const AudioTimeStamp& timestamp, UInt32 frames);

  // The retuned `AudioGlitchInfo` contains information gathered from all
  // `OnFramesReceived()` from the creation of the GlitchHelper or since
  // the last call to `ConsumeGlitchInfo()`.
  AudioGlitchInfo ConsumeGlitchInfo();

  // Log statistics gathered from calls to`OnFramesReceived()`. It should be
  // called when no more audio callbacks are expected. An human readable
  // summary of the generated statistics is returned if data was available.
  // `prefix` is added to the beginning of the returned string, it's purpose is
  // to differentiate strings from different users of the GlitchHelper.
  std::optional<std::string> LogAndReset(const std::string& prefix);

 private:
  bool HasData();

  const int sample_rate_;
  const AudioGlitchInfo::Direction direction_;
  SystemGlitchReporter glitch_reporter_;
  AudioGlitchInfo::Accumulator glitch_accumulator_;
  Float64 last_sample_time_ = 0;
  UInt32 last_number_of_frames_ = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_APPLE_GLITCH_HELPER_H_
