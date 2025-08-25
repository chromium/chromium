// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/language_detection/language_detection_model.h"

#include <map>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/language_detection/content/common/language_detection.mojom-blink.h"
#include "components/language_detection/content/renderer/language_detection_model_manager.h"
#include "components/language_detection/core/language_detection_model.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void LanguageDetectionModel::LoadModelFile(
    base::File model_file,
    CreateLanguageDetectionModelCallback callback) {
  language_detection_model_.UpdateWithFileAsync(
      std::move(model_file),
      blink::BindOnce(
          [](LanguageDetectionModel* model,
             CreateLanguageDetectionModelCallback callback) {
            if (!model || !model->language_detection_model_.IsAvailable()) {
              std::move(callback).Run(
                  base::unexpected(DetectLanguageError::kUnavailable));
              return;
            }
            std::move(callback).Run(model);
          },
          WrapWeakPersistent(this), std::move(callback)));
}

void LanguageDetectionModel::Trace(Visitor* visitor) const {}

void LanguageDetectionModel::DetectLanguage(
    scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const String& text,
    DetectLanguageCallback on_complete) {
  if (!language_detection_model_.IsAvailable()) {
    std::move(on_complete)
        .Run(base::unexpected(blink::DetectLanguageError::kUnavailable));
    return;
  }
  task_runner->PostTask(
      FROM_HERE,
      blink::BindOnce(&LanguageDetectionModel::DetectLanguageImpl,
                      WrapPersistent(this), text, std::move(on_complete)));
}

void LanguageDetectionModel::DetectLanguageImpl(
    const String& text,
    DetectLanguageCallback on_complete) {
  String text_16 = text;
  text_16.Ensure16Bit();
  auto score_by_language =
      language_detection_model_.PredictWithScan(text_16.View16());

  Vector<LanguagePrediction> predictions;
  predictions.reserve(static_cast<wtf_size_t>(score_by_language.size()));
  for (const auto& it : score_by_language) {
    predictions.emplace_back(it.language, it.score);
  }
  std::move(on_complete).Run(predictions);
}

int64_t LanguageDetectionModel::GetModelSize() const {
  if (!language_detection_model_.IsAvailable()) {
    return 0;
  }
  return language_detection_model_.GetModelSize();
}

}  // namespace blink
