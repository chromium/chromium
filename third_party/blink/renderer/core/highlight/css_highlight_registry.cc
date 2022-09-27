// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/highlight/css_highlight_registry.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

HighlightRegistry* CSSHighlightRegistry::highlights(ScriptState* script_state) {
  return HighlightRegistry::From(*ToLocalDOMWindow(script_state->GetContext()));
}

}  // namespace blink
