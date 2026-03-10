// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/chrome_ml_tool_types.h"

namespace ml {

ToolCall::ToolCall() = default;
ToolCall::~ToolCall() = default;
ToolCall::ToolCall(const ToolCall&) = default;
ToolCall& ToolCall::operator=(const ToolCall& other) = default;
ToolCall::ToolCall(ToolCall&&) = default;
ToolCall& ToolCall::operator=(ToolCall&& other) = default;

ToolResponse::ToolResponse() = default;
ToolResponse::~ToolResponse() = default;
ToolResponse::ToolResponse(const ToolResponse&) = default;
ToolResponse& ToolResponse::operator=(const ToolResponse& other) = default;
ToolResponse::ToolResponse(ToolResponse&&) = default;
ToolResponse& ToolResponse::operator=(ToolResponse&& other) = default;

ToolDeclaration::ToolDeclaration() = default;
ToolDeclaration::~ToolDeclaration() = default;
ToolDeclaration::ToolDeclaration(const ToolDeclaration&) = default;
ToolDeclaration& ToolDeclaration::operator=(const ToolDeclaration& other) =
    default;
ToolDeclaration::ToolDeclaration(ToolDeclaration&&) = default;
ToolDeclaration& ToolDeclaration::operator=(ToolDeclaration&& other) = default;

}  // namespace ml
