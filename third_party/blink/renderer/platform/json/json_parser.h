// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_JSON_JSON_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_JSON_JSON_PARSER_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <memory>

namespace blink {

class JSONValue;

enum class JSONParseErrorType {
  kNoError,
  kUnexpectedToken,
  kSyntaxError,
  kInvalidEscape,
  kTooMuchNesting,
  kUnexpectedDataAfterRoot,
  kUnsupportedEncoding,
};

struct PLATFORM_EXPORT JSONParseError {
  JSONParseErrorType type;
  int line;
  int column;
  String message;
};

// Parses |json| string and returns a value it represents.
// In case of parsing failure, returns nullptr.
// Optional error struct may be passed in, which will contain
// error details or |kNoError| if parsing succeeded.
// Optional boolean |opt_has_comments| may be passed in, which will contain
// whether |json| string contains comments.
PLATFORM_EXPORT std::unique_ptr<JSONValue> ParseJSON(
    const String& json,
    JSONParseError* opt_error = nullptr,
    bool* opt_has_comments = nullptr);

PLATFORM_EXPORT std::unique_ptr<JSONValue> ParseJSON(
    const String& json,
    int max_depth,
    JSONParseError* opt_error = nullptr,
    bool* opt_has_comments = nullptr);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_JSON_JSON_PARSER_H_
