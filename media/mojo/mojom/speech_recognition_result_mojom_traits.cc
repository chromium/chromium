// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/speech_recognition_result_mojom_traits.h"

namespace mojo {

namespace {

constexpr base::TimeDelta kZeroTime = base::Seconds(0);

}  // namespace

// static
bool StructTraits<
    media::mojom::HypothesisPartsDataView,
    media::HypothesisParts>::Read(media::mojom::HypothesisPartsDataView data,
                                  media::HypothesisParts* out) {
  std::vector<std::string> text;
  base::TimeDelta offset = kZeroTime;

  if (!data.ReadText(&text) || !data.ReadHypothesisPartOffset(&offset))
    return false;
  if (offset < kZeroTime)
    return false;

  out->text = std::move(text);
  out->hypothesis_part_offset = offset;
  return true;
}

bool StructTraits<media::mojom::TimingInformationDataView,
                  media::TimingInformation>::
    Read(media::mojom::TimingInformationDataView data,
         media::TimingInformation* out) {
  base::TimeDelta audio_start_time = kZeroTime;
  base::TimeDelta audio_end_time = kZeroTime;
  std::optional<std::vector<media::HypothesisParts>> hypothesis_parts;

  if (!data.ReadAudioStartTime(&audio_start_time) ||
      !data.ReadAudioEndTime(&audio_end_time) ||
      !data.ReadHypothesisParts(&hypothesis_parts)) {
    return false;
  }

  if (audio_start_time < kZeroTime || audio_end_time < audio_start_time)
    return false;

  if (hypothesis_parts.has_value() && hypothesis_parts->size() > 0) {
    base::TimeDelta prev_offset = kZeroTime;
    base::TimeDelta max_offset = audio_end_time - audio_start_time;
    for (const auto& part : *hypothesis_parts) {
      if (part.hypothesis_part_offset < prev_offset ||
          part.hypothesis_part_offset > max_offset) {
        return false;
      }
      prev_offset = part.hypothesis_part_offset;
    }
  }

  out->audio_start_time = audio_start_time;
  out->audio_end_time = audio_end_time;
  out->hypothesis_parts = std::move(hypothesis_parts);
  return true;
}

bool StructTraits<media::mojom::SpeechRecognitionResultDataView,
                  media::SpeechRecognitionResult>::
    Read(media::mojom::SpeechRecognitionResultDataView data,
         media::SpeechRecognitionResult* out) {
  std::string transcription;
  std::optional<media::TimingInformation> timing_information;

  if (!data.ReadTranscription(&transcription) ||
      !data.ReadTimingInformation(&timing_information)) {
    return false;
  }

  // Timing information is provided only for final results.
  if (!data.is_final() && timing_information.has_value())
    return false;

  out->transcription = std::move(transcription);
  out->is_final = data.is_final();
  out->timing_information = std::move(timing_information);
  return true;
}

}  // namespace mojo
