// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/speech_recognition_result.h"

namespace media {

HypothesisParts::HypothesisParts() = default;
HypothesisParts::HypothesisParts(const std::vector<std::string> part,
                                 base::TimeDelta offset)
    : text(part), hypothesis_part_offset(offset) {}

HypothesisParts::HypothesisParts(const HypothesisParts&) = default;
HypothesisParts::HypothesisParts(HypothesisParts&&) = default;
HypothesisParts& HypothesisParts::operator=(const HypothesisParts&) = default;
HypothesisParts& HypothesisParts::operator=(HypothesisParts&&) = default;
HypothesisParts::~HypothesisParts() = default;

bool HypothesisParts::operator==(const HypothesisParts& rhs) const {
  return text == rhs.text &&
         hypothesis_part_offset == rhs.hypothesis_part_offset;
}

TimingInformation::TimingInformation() = default;
TimingInformation::TimingInformation(const TimingInformation&) = default;
TimingInformation::TimingInformation(TimingInformation&&) = default;
TimingInformation& TimingInformation::operator=(const TimingInformation&) =
    default;
TimingInformation& TimingInformation::operator=(TimingInformation&&) = default;
TimingInformation::~TimingInformation() = default;

bool TimingInformation::operator==(const TimingInformation& rhs) const {
  return audio_start_time == rhs.audio_start_time &&
         audio_end_time == rhs.audio_end_time &&
         hypothesis_parts == rhs.hypothesis_parts;
}

SpeechRecognitionResult::SpeechRecognitionResult() = default;
SpeechRecognitionResult::SpeechRecognitionResult(const std::string transcript,
                                                 bool is_final)
    : transcription(transcript), is_final(is_final) {}

SpeechRecognitionResult::SpeechRecognitionResult(
    const SpeechRecognitionResult&) = default;
SpeechRecognitionResult::SpeechRecognitionResult(SpeechRecognitionResult&&) =
    default;
SpeechRecognitionResult& SpeechRecognitionResult::operator=(
    const SpeechRecognitionResult&) = default;
SpeechRecognitionResult& SpeechRecognitionResult::operator=(
    SpeechRecognitionResult&&) = default;
SpeechRecognitionResult::~SpeechRecognitionResult() = default;

bool SpeechRecognitionResult::operator==(
    const SpeechRecognitionResult& rhs) const {
  return transcription == rhs.transcription && is_final == rhs.is_final &&
         timing_information == rhs.timing_information;
}

}  // namespace media
