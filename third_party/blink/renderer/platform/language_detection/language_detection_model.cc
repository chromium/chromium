// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/language_detection/language_detection_model.h"

#include <map>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "components/language_detection/content/renderer/language_detection_model_manager.h"
#include "components/language_detection/core/language_detection_model.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace {

// Keep one model and one manager, shared across all blink uses.
// TODO(https://crbug.com/368206184): This will need to change to accommodate
// workers, as the model is not threadsafe.
language_detection::LanguageDetectionModelManager&
GetLanguageDetectionModelManager() {
  static base::NoDestructor<language_detection::LanguageDetectionModel> model;
  static base::NoDestructor<language_detection::LanguageDetectionModelManager>
      instance(*model);
  return *instance;
}

}  // namespace

namespace blink {

// static
void LanguageDetectionModel::Create(
    const blink::BrowserInterfaceBrokerProxy& interface_broker,
    CreateLanguageDetectionModelCallback callback) {
  GetLanguageDetectionModelManager().GetLanguageDetectionModel(
      interface_broker,
      WTF::BindOnce(OnModelResponseReceived, std::move(callback)));
}

void LanguageDetectionModel::OnModelResponseReceived(
    CreateLanguageDetectionModelCallback callback,
    language_detection::LanguageDetectionModel* model) {
  std::move(callback).Run(
      model ? MaybeModel(MakeGarbageCollected<LanguageDetectionModel>(*model))
            : base::unexpected(DetectLanguageError::kUnavailable));
}

LanguageDetectionModel::LanguageDetectionModel(
    const language_detection::LanguageDetectionModel& language_detection_model)
    : language_detection_model_(language_detection_model) {}

void LanguageDetectionModel::Trace(Visitor* visitor) const {}

void LanguageDetectionModel::DetectLanguage(
    const WTF::String& text,
    DetectLanguageCallback on_complete) {
  if (!language_detection_model_->IsAvailable()) {
    std::move(on_complete)
        .Run(base::unexpected(blink::DetectLanguageError::kUnavailable));
    return;
  }
  WTF::String text_16 = text;
  text_16.Ensure16Bit();
  auto score_by_language = language_detection_model_->PredictWithScan(
      std::u16string_view(text_16.Characters16(), text_16.length()));

  WTF::Vector<LanguagePrediction> predictions;
  predictions.reserve(static_cast<wtf_size_t>(score_by_language.size()));
  for (const auto& it : score_by_language) {
    predictions.emplace_back(it.language, it.score);
  }
  std::move(on_complete).Run(predictions);
}

}  // namespace blink
