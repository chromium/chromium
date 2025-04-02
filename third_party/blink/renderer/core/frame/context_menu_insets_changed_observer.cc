// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/context_menu_insets_changed_observer.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

ContextMenuInsetsChangedObserver::ContextMenuInsetsChangedObserver(
    LocalFrame& frame) {
  frame.RegisterContextMenuInsetsChangedObserver(this);
}

}  // namespace blink
