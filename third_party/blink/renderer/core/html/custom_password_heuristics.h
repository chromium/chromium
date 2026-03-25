// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_PASSWORD_HEURISTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_PASSWORD_HEURISTICS_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class ComputedStyle;
class String;

// Returns whether CSS masking via -webkit-text-security is active for an
// element's computed style.
CORE_EXPORT bool IsCSSSecurityMaskingEnabled(const ComputedStyle& style);

// Returns whether a JS-masked text value looks password-like and should be
// treated as sensitive (for example "••••a" or "•••••").
CORE_EXPORT bool IsLikelyJSCustomPasswordField(const String& value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_PASSWORD_HEURISTICS_H_
