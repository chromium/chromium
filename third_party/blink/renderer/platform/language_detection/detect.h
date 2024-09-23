// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LANGUAGE_DETECTION_DETECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LANGUAGE_DETECTION_DETECT_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/language_detection/core/language_detection_model.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class DetectLanguageError {
  // 0 intentionally skipped.
  // The model was not available for use.
  kUnavailable = 1,
};

using LanguagePrediction = language_detection::Prediction;
using DetectLanguageCallback = base::OnceCallback<void(
    base::expected<WTF::Vector<LanguagePrediction>, DetectLanguageError>)>;
// This uses the TFLite model to detect the languages contained in `text`.
// The model operates on a limited number characters at a time. This function
// splits `text` into chunks of characters within the limit and averages the
// model's results across all of the chunks. This has the effect of measuring
// what fraction of the string is in what language, even though the model was
// trained on monolingual inputs.
//
// This is an asynchronous operation. The result will be passed to the
// `on_complete` callback.
PLATFORM_EXPORT void DetectLanguage(const WTF::String& text,
                                    DetectLanguageCallback on_complete);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LANGUAGE_DETECTION_DETECT_H_
