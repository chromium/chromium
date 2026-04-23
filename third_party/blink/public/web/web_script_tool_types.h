// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_TOOL_TYPES_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_TOOL_TYPES_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

enum class WebScriptToolErrorCode {
  kInvalidToolName,
  kInvalidInputArguments,
  kMissingRequiredSubmitButton,
  kToolInvocationFailed,
  kToolCancelled,
};

struct WebScriptToolError {
  WebScriptToolErrorCode code;
  WebString message;

  explicit WebScriptToolError(WebScriptToolErrorCode code,
                              WebString message = WebString())
      : code(code), message(std::move(message)) {}

  bool operator==(const WebScriptToolError& other) const {
    return code == other.code;
  }
  bool operator==(WebScriptToolErrorCode other_code) const {
    return code == other_code;
  }
};

struct WebScriptToolDeclaration {
  WebString description;
  WebString input_schema;
  std::optional<bool> read_only;
  std::optional<bool> untrusted_content;
};

using WebScriptToolResultCallback =
    base::OnceCallback<void(std::unique_ptr<WebScriptToolDeclaration>,
                            base::expected<WebString, WebScriptToolError>)>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_TOOL_TYPES_H_
