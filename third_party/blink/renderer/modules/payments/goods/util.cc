// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/goods/util.h"

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace digital_goods_util {

void LogConsoleError(ScriptState* script_state, const String& message) {
  if (!script_state || !script_state->ContextIsValid()) {
    VLOG(1) << message;
    return;
  }
  auto* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);
  execution_context->AddConsoleMessage(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kError, message,
      /*discard_duplicates=*/true);
}

}  // namespace digital_goods_util
}  // namespace blink
