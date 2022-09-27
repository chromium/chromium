// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/css_paint_worklet.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
Worklet* CSSPaintWorklet::paintWorklet(ScriptState* script_state) {
  return PaintWorklet::From(*ToLocalDOMWindow(script_state->GetContext()));
}

}  // namespace blink
