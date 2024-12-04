// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator_capabilities.h"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "base/check_op.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_translation_availability.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

// Returns the index of the language category that contains the given language,
// or std::nullopt if the language is not found.
std::optional<size_t> GetLanguageCategoryIndex(
    const Vector<Vector<mojom::blink::TranslatorLanguageCodePtr>>&
        language_categories,
    const String& language) {
  for (size_t i = 0; i < language_categories.size(); ++i) {
    for (const auto& language_code : language_categories[i]) {
      if (language_code->code == language) {
        return i;
      }
    }
  }
  return std::nullopt;
}

// Converts a mojom::blink::TranslationAvailability to a
// V8AICapabilityAvailability.
V8AICapabilityAvailability ToV8AICapabilityAvailability(
    mojom::blink::TranslationAvailability availability) {
  switch (availability) {
    case mojom::blink::TranslationAvailability::kNo:
      return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
    case mojom::blink::TranslationAvailability::kReadily:
      return V8AICapabilityAvailability(
          V8AICapabilityAvailability::Enum::kReadily);
    case mojom::blink::TranslationAvailability::kAfterDownload:
      return V8AICapabilityAvailability(
          V8AICapabilityAvailability::Enum::kAfterDownload);
  }
}

}  // namespace

AITranslatorCapabilities::AITranslatorCapabilities(
    mojom::blink::TranslatorAvailabilityInfoPtr info)
    : info_(std::move(info)) {
  CHECK_EQ(info_->language_availability_matrix.size(),
           info_->language_categories.size());
  for (const auto& matrix_row : info_->language_availability_matrix) {
    CHECK_EQ(matrix_row.size(), info_->language_categories.size());
  }
}

V8AICapabilityAvailability AITranslatorCapabilities::available() const {
  return ToV8AICapabilityAvailability(info_->availability);
}

V8AICapabilityAvailability AITranslatorCapabilities::languagePairAvailable(
    const String& source,
    const String& target) {
  // When source and target are the same, the translation is not available.
  if (source == target) {
    return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
  }

  const std::optional<size_t> source_category =
      GetLanguageCategoryIndex(info_->language_categories, source);
  if (!source_category) {
    // If the source language is not found, the translation is not available.
    return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
  }
  const std::optional<size_t> target_category =
      GetLanguageCategoryIndex(info_->language_categories, target);
  if (!target_category) {
    // If the target language is not found, the translation is not available.
    return V8AICapabilityAvailability(V8AICapabilityAvailability::Enum::kNo);
  }
  // Get the availability from the availability matrix.
  return ToV8AICapabilityAvailability(
      info_->language_availability_matrix[*source_category][*target_category]);
}

void AITranslatorCapabilities::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
