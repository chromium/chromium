// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_url_pattern_to_safe_url_pattern.h"

#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

namespace blink {

std::optional<SafeUrlPattern> WebURLPatternToSafeUrlPattern(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  NonThrowableExceptionState exception_state;

  URLPattern* url_pattern = nullptr;
  if (value->IsString()) {
    url_pattern = URLPattern::Create(isolate,
                                     V8UnionURLPatternInitOrUSVString::Create(
                                         isolate, value, exception_state),
                                     exception_state);
  } else {
    url_pattern = NativeValueTraits<URLPattern>::NativeValue(isolate, value,
                                                             exception_state);
  }

  if (!url_pattern) {
    return std::nullopt;
  }

  return url_pattern->ToSafeUrlPattern(exception_state);
}

}  // namespace blink
