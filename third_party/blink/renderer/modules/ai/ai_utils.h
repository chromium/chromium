// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_

#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"

namespace blink {

// Converts string language codes to AILanguageCode mojo struct.
Vector<mojom::blink::AILanguageCodePtr> ToMojoLanguageCodes(
    const Vector<String>& language_codes);

// Converts AILanguageCode mojo struct to string language codes.
Vector<String> ToStringLanguageCodes(
    const Vector<mojom::blink::AILanguageCodePtr>& language_codes);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_UTILS_H_
