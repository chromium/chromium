// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_URL_PATTERN_TO_SAFE_URL_PATTERN_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_URL_PATTERN_TO_SAFE_URL_PATTERN_H_

#include "third_party/blink/public/platform/web_common.h"
#include "v8/include/v8-local-handle.h"

namespace v8 {
class Isolate;
class Value;
}  // namespace v8

namespace blink {

struct SafeUrlPattern;

// This function turns the URLPattern object or string coming from JS
// to a SafeUrlPattern. It exists because the URLPattern isn't exposed outside
// of Blink, so a jump here happens to have the URLPattern available and then
// we jump back with the publicly available SafeUrlPattern.
BLINK_EXPORT std::optional<SafeUrlPattern> WebURLPatternToSafeUrlPattern(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_URL_PATTERN_TO_SAFE_URL_PATTERN_H_
