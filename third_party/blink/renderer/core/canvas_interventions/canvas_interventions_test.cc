// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/canvas_interventions_test.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

// static
String CanvasInterventionsTest::getCanvasNoiseToken(
    ExecutionContext* execution_context) {
  std::optional<NoiseToken> token = execution_context->CanvasNoiseToken();
  if (token.has_value()) {
    return String::Number(token->Value());
  }
  return String();
}

}  // namespace blink
