// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/css_layout_worklet.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/custom/layout_worklet.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
Worklet* CSSLayoutWorklet::layoutWorklet(ScriptState* script_state) {
  return LayoutWorklet::From(*ToLocalDOMWindow(script_state->GetContext()));
}

}  // namespace blink
