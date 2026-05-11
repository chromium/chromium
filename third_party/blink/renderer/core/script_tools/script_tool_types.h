// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_TYPES_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class ScriptToolErrorCode {
  kInvalidToolName,
  kInvalidInputArguments,
  kMissingRequiredSubmitButton,
  kToolInvocationFailed,
  kToolCancelled,
};

struct ScriptToolError {
  ScriptToolErrorCode code;
  String message;

  explicit ScriptToolError(ScriptToolErrorCode code, String message = String())
      : code(code), message(std::move(message)) {}

  bool operator==(const ScriptToolError& other) const {
    return code == other.code;
  }
  bool operator==(ScriptToolErrorCode other_code) const {
    return code == other_code;
  }
};

// TODO(https://crbug.com/506393880): Move this to the `model_context.cc`
// anonymous namespace, when `model_context_testing.cc` stops relying on it.
inline String GetToolErrorMessage(const ScriptToolError& error) {
  if (!error.message.empty()) {
    return error.message;
  }
  String conversion;
  switch (error.code) {
    case ScriptToolErrorCode::kInvalidToolName:
      conversion = "Tool was not executed due to invalid name";
      break;
    case ScriptToolErrorCode::kInvalidInputArguments:
      conversion = "Tool was not executed due to invalid input arguments";
      break;
    case ScriptToolErrorCode::kMissingRequiredSubmitButton:
      conversion =
          "Tool was not executed due to missing required submit button";
      break;
    case ScriptToolErrorCode::kToolInvocationFailed:
      conversion =
          "Tool was executed but the invocation failed. For example, the "
          "script function threw an error";
      break;
    case ScriptToolErrorCode::kToolCancelled:
      conversion = "Tool was cancelled";
      break;
    default:
      conversion = "Unknown failure";
  }
  return conversion;
}

struct ScriptToolDeclaration {
  String description;
  String input_schema;
  std::optional<bool> read_only;
  std::optional<bool> untrusted_content;
};

using ScriptToolExecutedCallback =
    base::OnceCallback<void(base::expected<String, ScriptToolError>)>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_SCRIPT_TOOL_TYPES_H_
