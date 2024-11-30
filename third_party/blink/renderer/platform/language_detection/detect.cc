// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/language_detection/detect.h"

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "components/language_detection/core/language_detection_model.h"
#include "components/language_detection/core/language_detection_provider.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace {

void DetectLanguageWithModel(
    const WTF::String& text,
    blink::DetectLanguageCallback on_complete,
    language_detection::LanguageDetectionModel& model) {
  if (!model.IsAvailable()) {
    std::move(on_complete)
        .Run(base::unexpected(blink::DetectLanguageError::kUnavailable));
    return;
  }
  WTF::String content = text;
  content.Ensure16Bit();
  std::vector<language_detection::Prediction> predictions =
      model.PredictWithScan(
          std::u16string_view(content.Characters16(), content.length()));

  WTF::Vector<blink::LanguagePrediction> blink_predictions;
  blink_predictions.ReserveInitialCapacity(
      static_cast<wtf_size_t>(predictions.size()));
  for (const auto& it : predictions) {
    blink_predictions.emplace_back(it.language, it.score);
  }
  std::move(on_complete).Run(std::move(blink_predictions));
}

}  // namespace

namespace blink {

void DetectLanguage(const WTF::String& text,
                    DetectLanguageCallback on_complete) {
  auto& model = language_detection::GetLanguageDetectionModel();
  model.AddOnModelLoadedCallback(
      WTF::BindOnce(DetectLanguageWithModel, text, std::move(on_complete)));
}

}  // namespace blink
