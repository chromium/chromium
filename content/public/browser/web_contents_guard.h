// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_GUARD_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_GUARD_H_

#include "base/auto_reset.h"
#include "base/memory/stack_allocated.h"
#include "content/common/content_export.h"

namespace content {

class WebContents;

// Ensures that the given contents does not cease to exist during the lifetime
// of the guard object. Used to enforce invariants in the codebase where it is
// better to crash than to risk memory corruption - i.e. when conceivably some
// callback *could* result in the contents' destruction but no *reasonable*
// circumstance should actually allow it.
//
// Similar to raw_ref and raw_ptr in that they guard against UAFs with process
// death, but WebContentsGuard provides immediate protection against unexpected
// destruction even in deeply-nested calls - there's literally no time to
// dereference an invalid pointer since the protected object is valid all the
// way up until the CHECK().
//
// Example:
// ```
//  void ShowPopupBlockedDialog(Tab* tab, WebContents* contents) {
//    WebContentsGuard guard(*contents);
//    auto dialog = std::make_unique<PopupBlockedDialog>(tab, contents);
//    PopupBlockedDialog::ShowBubble(std::move(dialog));
//  }
// ```
//
// If a guard leads to a CHECK(), triage by:
//  1. Checking the call stack. Does it make sense that this series of callbacks
//     should have destroyed the `WebContents` in this situation?
//  2. If not, fix the issue.
//  3. If the `WebContents` must be destroyed, either:
//     a) Defer destruction of the WebContents to the next frame.
//     b) Guard accesses of the WebContents with WeakPtr and document why it's
//        valid for the WebContents to be destroyed.
//
class CONTENT_EXPORT [[maybe_unused, nodiscard]] WebContentsGuard {
  // Guards a defined scope, so must be created on the stack.
  STACK_ALLOCATED();

 public:
  explicit WebContentsGuard(WebContents& contents);
  WebContentsGuard(const WebContentsGuard& other) = delete;
  WebContentsGuard& operator=(const WebContentsGuard& other) = delete;
  ~WebContentsGuard();

 private:
  // This uses the existing `WebContentsImpl::prevent_destruction_` flag.
  // Because this flag is only ever set within scoped `AutoReset`s, there is no
  // issue with nesting guards (the inner guards will simply exchange two `true`
  // values).
  base::AutoReset<bool> guard_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_GUARD_H_
