// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_UTILS_H_

#include "base/types/expected.h"

namespace v8 {
class Isolate;
}  // namespace v8

namespace blink {
class ExceptionState;
class JSONValue;
class KURL;
class String;
class URLPattern;

base::expected<URLPattern*, String> ParseURLPatternFromJSON(
    v8::Isolate*,
    const JSONValue&,
    const KURL& base_url,
    ExceptionState&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_UTILS_H_
