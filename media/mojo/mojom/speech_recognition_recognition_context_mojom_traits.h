// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RECOGNITION_CONTEXT_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RECOGNITION_CONTEXT_MOJOM_TRAITS_H_

#include "media/mojo/mojom/speech_recognition_recognition_context.h"
#include "media/mojo/mojom/speech_recognition_recognition_context.mojom.h"

namespace mojo {

template <>
class StructTraits<media::mojom::SpeechRecognitionPhraseDataView,
                   media::SpeechRecognitionPhrase> {
 public:
  static const std::string& phrase(const media::SpeechRecognitionPhrase& r) {
    return r.phrase;
  }

  static float boost(const media::SpeechRecognitionPhrase& r) {
    return r.boost;
  }

  static bool Read(media::mojom::SpeechRecognitionPhraseDataView data,
                   media::SpeechRecognitionPhrase* out);
};

template <>
class StructTraits<media::mojom::SpeechRecognitionRecognitionContextDataView,
                   media::SpeechRecognitionRecognitionContext> {
 public:
  static const std::vector<media::SpeechRecognitionPhrase>& phrases(
      const media::SpeechRecognitionRecognitionContext& r) {
    return r.phrases;
  }

  static bool Read(
      media::mojom::SpeechRecognitionRecognitionContextDataView data,
      media::SpeechRecognitionRecognitionContext* out);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_SPEECH_RECOGNITION_RECOGNITION_CONTEXT_MOJOM_TRAITS_H_
