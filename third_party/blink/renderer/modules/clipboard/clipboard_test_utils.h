// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_TEST_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

// This is a helper class which provides utility methods
// for testing the Async Clipboard API.
class ClipboardTestBase : public PageTestBase {
 public:
  void SetPageFocus(bool focused) {
    GetPage().GetFocusController().SetActive(focused);
    GetPage().GetFocusController().SetFocused(focused);
  }

  void SetSecureOrigin(ExecutionContext* executionContext) {
    KURL page_url("https://example.com");
    scoped_refptr<SecurityOrigin> page_origin =
        SecurityOrigin::Create(page_url);
    executionContext->GetSecurityContext().SetSecurityOriginForTesting(nullptr);
    executionContext->GetSecurityContext().SetSecurityOrigin(page_origin);
  }

  void WritePlainTextToClipboard(const String& text, V8TestingScope& scope) {
    scope.GetFrame().GetSystemClipboard()->WritePlainText(text);
  }
};

class EventCountingListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override { count_++; }

  int Count() const { return count_; }

 private:
  int count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_TEST_UTILS_H_
