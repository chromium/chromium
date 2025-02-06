// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_utils.h"

#include <algorithm>
#include <iterator>

namespace blink {

Vector<mojom::blink::AILanguageCodePtr> ToMojoLanguageCodes(
    const Vector<String>& language_codes) {
  Vector<mojom::blink::AILanguageCodePtr> result;
  result.reserve(language_codes.size());
  std::ranges::transform(
      language_codes, std::back_inserter(result),
      [](const String& language_code) {
        return mojom::blink::AILanguageCode::New(language_code);
      });
  return result;
}

}  // namespace blink
