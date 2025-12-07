// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_OPTIONS_H_

#include "base/check.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_options.h"

namespace blink {

class URLPatternOptions;

namespace url_pattern {

// A struct corresponds to `URLPatternOptions` in url_pattern_options.idl.
struct Options {
  bool ignore_case = false;

  static Options FromV8URLPatternOptions(const URLPatternOptions* options) {
    CHECK(options);
    CHECK(options->hasIgnoreCase());

    return {.ignore_case = options->ignoreCase()};
  }
};

}  // namespace url_pattern
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_OPTIONS_H_
