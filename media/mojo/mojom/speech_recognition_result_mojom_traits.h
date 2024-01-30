// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_MOJOM_TRAITS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace mojo {

template <>
class StructTraits<media::mojom::HypothesisPartsDataView,
                   media::HypothesisParts> {
 public:
  static const std::vector<std::string>& text(const media::HypothesisParts& r) {
    return r.text;
  }

  static base::TimeDelta hypothesis_part_offset(
      const media::HypothesisParts& r) {
    return r.hypothesis_part_offset;
  }

  static bool Read(media::mojom::HypothesisPartsDataView data,
                   media::HypothesisParts* out);
};

template <>
class StructTraits<media::mojom::TimingInformationDataView,
                   media::TimingInformation> {
 public:
  static base::TimeDelta audio_start_time(const media::TimingInformation& r) {
    return r.audio_start_time;
  }

  static base::TimeDelta audio_end_time(const media::TimingInformation& r) {
    return r.audio_end_time;
  }

  static const ::std::optional<std::vector<media::HypothesisParts>>&
  hypothesis_parts(const media::TimingInformation& r) {
    return r.hypothesis_parts;
  }

  static bool Read(media::mojom::TimingInformationDataView data,
                   media::TimingInformation* out);
};

template <>
class StructTraits<media::mojom::SpeechRecognitionResultDataView,
                   media::SpeechRecognitionResult> {
 public:
  static const std::string& transcription(
      const media::SpeechRecognitionResult& r) {
    return r.transcription;
  }

  static bool is_final(const media::SpeechRecognitionResult& r) {
    return r.is_final;
  }

  static const ::std::optional<media::TimingInformation>& timing_information(
      const media::SpeechRecognitionResult& r) {
    return r.timing_information;
  }

  static bool Read(media::mojom::SpeechRecognitionResultDataView data,
                   media::SpeechRecognitionResult* out);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RESULT_MOJOM_TRAITS_H_
