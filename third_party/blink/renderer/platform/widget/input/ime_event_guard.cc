// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/ime_event_guard.h"

#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

// When ThreadedInputConnection is used, we want to make sure that FROM_IME
// is set only for OnRequestTextInputStateUpdate() so that we can distinguish
// it from other updates so that we can wait for it safely. So it is false by
// default.
ImeEventGuard::ImeEventGuard(base::WeakPtr<WidgetBase> widget)
    : widget_(std::move(widget)) {
  widget_->OnImeEventGuardStart(this);
}

ImeEventGuard::~ImeEventGuard() {
  if (widget_)
    widget_->OnImeEventGuardFinish(this);
}

}  //  namespace blink
