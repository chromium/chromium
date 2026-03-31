// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_PASSWORD_HEURISTICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_PASSWORD_HEURISTICS_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class String;

// Returns whether a JS-masked text value looks password-like and should be
// treated as sensitive (for example "••••a" or "•••••").
CORE_EXPORT bool IsLikelyJSCustomPasswordField(const String& value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_PASSWORD_HEURISTICS_H_
