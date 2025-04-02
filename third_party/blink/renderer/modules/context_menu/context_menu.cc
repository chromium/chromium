// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/context_menu/context_menu.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/css/document_style_environment_variables.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "ui/gfx/geometry/insets.h"

namespace blink {

// static
const char ContextMenu::kSupplementName[] = "ContextMenu";

// static
ContextMenu* ContextMenu::From(LocalFrame& frame) {
  return Supplement<LocalFrame>::From<ContextMenu>(frame);
}

// static
void ContextMenu::ProvideTo(LocalFrame& frame) {
  if (ContextMenu::From(frame)) {
    return;
  }
  Supplement<LocalFrame>::ProvideTo(frame,
                                    MakeGarbageCollected<ContextMenu>(frame));
}

ContextMenu::ContextMenu(LocalFrame& frame)
    : Supplement<LocalFrame>(frame),
      ContextMenuInsetsChangedObserver(frame),
      frame_(frame) {}

void ContextMenu::ContextMenuInsetsChanged(const gfx::Insets* insets) {
  TRACE_EVENT0("vk", "ContextMenu::ContextMenuInsetsChanged");
  if (!frame_) {
    return;
  }
  DCHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled(
      frame_->DomWindow()));
  DocumentStyleEnvironmentVariables& vars = frame_->DomWindow()
                                                ->document()
                                                ->GetStyleEngine()
                                                .EnsureEnvironmentVariables();
  if (insets) {
    vars.SetVariable(UADefinedVariable::kContextMenuInsetTop,
                     StyleEnvironmentVariables::FormatFloatPx(insets->top()));
    vars.SetVariable(UADefinedVariable::kContextMenuInsetLeft,
                     StyleEnvironmentVariables::FormatFloatPx(insets->left()));
    vars.SetVariable(
        UADefinedVariable::kContextMenuInsetBottom,
        StyleEnvironmentVariables::FormatFloatPx(insets->bottom()));
    vars.SetVariable(UADefinedVariable::kContextMenuInsetRight,
                     StyleEnvironmentVariables::FormatFloatPx(insets->right()));
  } else {
    vars.RemoveVariable(UADefinedVariable::kContextMenuInsetTop);
    vars.RemoveVariable(UADefinedVariable::kContextMenuInsetLeft);
    vars.RemoveVariable(UADefinedVariable::kContextMenuInsetBottom);
    vars.RemoveVariable(UADefinedVariable::kContextMenuInsetRight);
  }
}

void ContextMenu::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  ContextMenuInsetsChangedObserver::Trace(visitor);
  Supplement<LocalFrame>::Trace(visitor);
}

}  // namespace blink
