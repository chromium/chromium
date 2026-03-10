// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TOOL_TYPES_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TOOL_TYPES_H_

#include <string>

namespace ml {

// Represents a tool call requested by the model during generation.
// This is a lightweight wrapper that holds the tool invocation request.
struct ToolCall {
  ToolCall();
  ~ToolCall();
  ToolCall(const ToolCall&);
  ToolCall& operator=(const ToolCall& other);
  ToolCall(ToolCall&&);
  ToolCall& operator=(ToolCall&& other);

  std::string call_id;
  std::string name;
  // JSON-serialized arguments.
  std::string arguments_json;
};

// Represents a tool response sent back after tool execution.
struct ToolResponse {
  ToolResponse();
  ~ToolResponse();
  ToolResponse(const ToolResponse&);
  ToolResponse& operator=(const ToolResponse& other);
  ToolResponse(ToolResponse&&);
  ToolResponse& operator=(ToolResponse&& other);

  std::string call_id;
  std::string name;
  // JSON-serialized result data (empty if error).
  std::string result_json;
  // Error message (empty if success).
  std::string error_message;
};

// Represents a tool declaration provided by the API client.
struct ToolDeclaration {
  ToolDeclaration();
  ~ToolDeclaration();
  ToolDeclaration(const ToolDeclaration&);
  ToolDeclaration& operator=(const ToolDeclaration& other);
  ToolDeclaration(ToolDeclaration&&);
  ToolDeclaration& operator=(ToolDeclaration&& other);

  std::string name;
  std::string description;
  // JSON-serialized input schema.
  std::string input_schema_json;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_TOOL_TYPES_H_
