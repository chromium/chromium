// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focus_changed_observer.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

FocusChangedObserver::FocusChangedObserver(Page* page) {
  DCHECK(page);
  page->GetFocusController().RegisterFocusChangedObserver(this);
}

bool FocusChangedObserver::IsFrameFocused(LocalFrame* frame) {
  if (!frame)
    return false;
  Page* page = frame->GetPage();
  if (!page)
    return false;
  const FocusController& controller = page->GetFocusController();
  return controller.IsFocused() && (controller.FocusedFrame() == frame);
}

}  // namespace blink
