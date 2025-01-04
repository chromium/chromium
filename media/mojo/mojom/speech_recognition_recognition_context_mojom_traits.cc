// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/speech_recognition_recognition_context_mojom_traits.h"

namespace mojo {

bool StructTraits<media::mojom::SpeechRecognitionPhraseDataView,
                  media::SpeechRecognitionPhrase>::
    Read(media::mojom::SpeechRecognitionPhraseDataView data,
         media::SpeechRecognitionPhrase* out) {
  std::string phrase;

  if (!data.ReadPhrase(&phrase) || phrase.empty()) {
    return false;
  }

  if (data.boost() < 0 || data.boost() > 10) {
    return false;
  }

  out->phrase = std::move(phrase);
  out->boost = data.boost();
  return true;
}

bool StructTraits<media::mojom::SpeechRecognitionRecognitionContextDataView,
                  media::SpeechRecognitionRecognitionContext>::
    Read(media::mojom::SpeechRecognitionRecognitionContextDataView data,
         media::SpeechRecognitionRecognitionContext* out) {
  std::vector<media::SpeechRecognitionPhrase> phrases;

  if (!data.ReadPhrases(&phrases)) {
    return false;
  }

  out->phrases = std::move(phrases);
  return true;
}

}  // namespace mojo
