// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/language_detection/detect.h"

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

// TODO(https://crbug.com/354070625): This should be exported from the component
// as a constant.
const unsigned kModelInputMaxChars = 128;

void DetectLanguage(
    const WTF::String& text,
    base::OnceCallback<void(base::expected<WTF::Vector<LanguagePrediction>,
                                           DetectLanguageError>)> on_complete) {
  const auto& model = language_detection::GetLanguageDetectionModel();
  if (!model.IsAvailable()) {
    std::move(on_complete)
        .Run(base::unexpected(DetectLanguageError::kUnavailable));
    return;
  }
  std::map<std::string, double> score_by_language;

  // Call the model on the entire string in chunks of kModelInputMaxChars and
  // average the reliabilty score across all of the calls.
  wtf_size_t pos = 0;
  size_t count = 0;
  while (pos < text.length()) {
    WTF::String substring = text.Substring(pos, kModelInputMaxChars);
    pos += kModelInputMaxChars;
    count++;
    substring.Ensure16Bit();
    auto predictions = model.Predict(
        std::u16string(substring.Characters16(), substring.length()));
    for (const auto& prediction : predictions) {
      score_by_language[prediction.language] += prediction.score;
    }
  }

  WTF::Vector<LanguagePrediction> predictions;
  predictions.reserve(static_cast<wtf_size_t>(score_by_language.size()));
  for (const auto& it : score_by_language) {
    predictions.emplace_back(it.first, it.second / count);
  }
  std::move(on_complete).Run(predictions);
}

}  // namespace blink
