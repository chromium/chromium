// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_GLITCH_INFO_H_
#define MEDIA_BASE_AUDIO_GLITCH_INFO_H_

#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// Audio glitch info.
struct MEDIA_EXPORT AudioGlitchInfo {
  // Total glitch duration. For playout this is the duration of inserted
  // silence.
  base::TimeDelta duration;

  // Number of glitches.
  unsigned int count = 0;

  // Stringifies the info for human-readable logging.
  std::string ToString() const;

  // Logs an "audio" category trace event in case glitch count is positive.
  void MaybeAddTraceEvent() const;

  AudioGlitchInfo& operator+=(const AudioGlitchInfo& other);

  class MEDIA_EXPORT Accumulator;

  enum class Direction { kRender, kCapture };

 private:
  friend class AUHALStream;
  friend class AUAudioInputStream;
  friend class CrasInputStream;
  friend class CrasUnifiedStream;
  friend class WASAPIAudioInputStream;
  friend class WASAPIAudioOutputStream;
  friend class AudioGlitchInfoTester;

  // Creates a glitch with duration clamped to between 0 and 1 seconds. Also
  // logs the glitch duration to a UMA-histogram. This should only be used in OS
  // audio stream implementations.
  static AudioGlitchInfo SingleBoundedSystemGlitch(
      const base::TimeDelta duration,
      const Direction direction);
};

MEDIA_EXPORT bool operator==(const AudioGlitchInfo& lhs,
                             const AudioGlitchInfo& rhs);

// Helper class used to accumulate pending AudioGlitchInfo, and reset it when
// the accumulated info is read.
class AudioGlitchInfo::Accumulator {
 public:
  Accumulator(const Accumulator&) = delete;
  Accumulator& operator=(const Accumulator&) = delete;
  Accumulator();
  ~Accumulator();

  void Add(const AudioGlitchInfo& info);

  AudioGlitchInfo GetAndReset();

 private:
  AudioGlitchInfo pending_info_;
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_GLITCH_INFO_H_
