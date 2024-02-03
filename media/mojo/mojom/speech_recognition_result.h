// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_H_
#define MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace media {

struct HypothesisParts {
  HypothesisParts();
  HypothesisParts(const std::vector<std::string> part, base::TimeDelta offset);
  HypothesisParts(const HypothesisParts&);
  HypothesisParts(HypothesisParts&&);
  HypothesisParts& operator=(const HypothesisParts&);
  HypothesisParts& operator=(HypothesisParts&&);
  ~HypothesisParts();

  bool operator==(const HypothesisParts& rhs) const;

  // A section of the final transcription text. Either an entire word or single
  // character (depending on the language) with adjacent punctuation. There will
  // usually only be one value here. If formatting is enabled in the speech
  // recognition, then the raw text will be included as the second element.
  std::vector<std::string> text;

  // Time offset from this event's |audio_start_time| defined below. Time
  // offset from this event's |audio_start_time| defined below. We enforce the
  // following invariant: 0 <= hypothesis_part_offset < |audio_end_time -
  // audio_start_time|.
  base::TimeDelta hypothesis_part_offset;
};

struct TimingInformation {
  TimingInformation();
  TimingInformation(const TimingInformation&);
  TimingInformation(TimingInformation&&);
  TimingInformation& operator=(const TimingInformation&);
  TimingInformation& operator=(TimingInformation&&);
  ~TimingInformation();

  bool operator==(const TimingInformation& rhs) const;

  // Start time in audio time from the start of the SODA session.
  // This time measures the amount of audio input into SODA.
  base::TimeDelta audio_start_time;

  // Elapsed processed audio from first frame after preamble.
  base::TimeDelta audio_end_time;

  // The timing information for each word/letter in the transription.
  // HypothesisPartsInResult was introduced in min version 1 in
  // chromeos/services/machine_learning/public/mojom/soda.mojom. Therefore, it
  // must be optional. Hypothesis parts maybe non-empty optional containing a
  // zero length vector if no words were spoken during the event's time span.
  std::optional<std::vector<HypothesisParts>> hypothesis_parts;
};

// A speech recognition result created by the speech service and passed to the
// SpeechRecognitionRecognizerClient.
struct SpeechRecognitionResult {
  SpeechRecognitionResult();
  SpeechRecognitionResult(const std::string transcript, bool is_final);
  SpeechRecognitionResult(const SpeechRecognitionResult&);
  SpeechRecognitionResult(SpeechRecognitionResult&&);
  SpeechRecognitionResult& operator=(const SpeechRecognitionResult&);
  SpeechRecognitionResult& operator=(SpeechRecognitionResult&&);
  ~SpeechRecognitionResult();

  bool operator==(const SpeechRecognitionResult& rhs) const;

  std::string transcription;

  // A flag indicating whether the result is final. If true, the result is
  // locked in and the next result returned will not overlap with the previous
  // final result.
  bool is_final = false;

  // Timing information for the current transcription.
  std::optional<TimingInformation> timing_information;
};

}  // namespace media

#endif  // MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_H_
