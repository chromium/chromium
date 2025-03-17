// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_

#include "base/types/expected.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_create_core_options.h"

namespace blink {

// Converts string language codes to AILanguageCode mojo struct.
Vector<mojom::blink::AILanguageCodePtr> ToMojoLanguageCodes(
    const Vector<String>& language_codes);

// Converts AILanguageCode mojo struct to string language codes.
Vector<String> ToStringLanguageCodes(
    const Vector<mojom::blink::AILanguageCodePtr>& language_codes);

enum class SamplingParamsOptionError {
  kOnlyOneOfTopKAndTemperatureIsProvided,
  kInvalidTopK,
  kInvalidTemperature,
};
// Performs check on the sampling params option and return the constructed
// AILanguageModelSamplingParamsPtr if the option is valid, or a DOMException
// otherwise.
MODULES_EXPORT base::expected<mojom::blink::AILanguageModelSamplingParamsPtr,
                              SamplingParamsOptionError>
ResolveSamplingParamsOption(const AILanguageModelCreateCoreOptions* options);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_
