// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_FEEDBACK_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_FEEDBACK_HELPERS_H_

#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// Logs a console message requesting feedback for the given AI session type,
// ensuring the message is only logged once per ExecutionContext.
void MaybeRequestFeedback(ScriptState* script_state,
                          AIMetrics::AISessionType session_type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_FEEDBACK_HELPERS_H_
