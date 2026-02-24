// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_REDACTION_HEURISTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_REDACTION_HEURISTICS_H_

namespace blink {

class LayoutObject;
class String;

// Returns whether CSS masking via -webkit-text-security is active for an
// element's computed style.
bool IsCSSSecurityMaskingEnabled(const LayoutObject& object);

// Returns whether a JS-masked text value looks password-like and should be
// treated as sensitive (for example "••••a" or "•••••").
bool IsLikelyJSCustomPasswordField(const String& value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_EXTRACTION_AI_PAGE_CONTENT_REDACTION_HEURISTICS_H_
