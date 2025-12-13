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

bool StructTraits<media::mojom::MediaTimestampRangeDataView,
                  media::MediaTimestampRange>::
    Read(media::mojom::MediaTimestampRangeDataView data,
         media::MediaTimestampRange* out) {
  base::TimeDelta start;
  base::TimeDelta end;

  if (!data.ReadStart(&start) || !data.ReadEnd(&end)) {
    return false;
  }

  if (start >= end) {
    return false;
  }

  out->start = start;
  out->end = end;
  return true;
}

bool StructTraits<media::mojom::TimingInformationDataView,
                  media::TimingInformation>::
    Read(media::mojom::TimingInformationDataView data,
         media::TimingInformation* out) {
  base::TimeDelta audio_start_time = kZeroTime;
  base::TimeDelta audio_end_time = kZeroTime;
  std::optional<std::vector<media::HypothesisParts>> hypothesis_parts;
  std::optional<std::vector<media::MediaTimestampRange>>
      originating_media_timestamps;

  if (!data.ReadAudioStartTime(&audio_start_time) ||
      !data.ReadAudioEndTime(&audio_end_time) ||
      !data.ReadHypothesisParts(&hypothesis_parts) ||
      !data.ReadOriginatingMediaTimestamps(&originating_media_timestamps)) {
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

  // `originating_media_timestamps` should have already failed to deserialize if
  // any of its MediaTimestampRanges has a start a `start` >= `end`.

  out->audio_start_time = audio_start_time;
  out->audio_end_time = audio_end_time;
  out->hypothesis_parts = std::move(hypothesis_parts);
  out->originating_media_timestamps = std::move(originating_media_timestamps);
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

  out->transcription = std::move(transcription);
  out->is_final = data.is_final();
  out->timing_information = std::move(timing_information);
  return true;
}

}  // namespace mojo
