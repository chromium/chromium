// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_change_event_controller.h"

#include "third_party/blink/renderer/modules/clipboard/clipboard_test_utils.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class ClipboardChangeEventTest : public ClipboardTestBase {};

TEST_F(ClipboardChangeEventTest, ClipboardChangeEventFiresWhenFocused) {
  ExecutionContext* executionContext = GetFrame().DomWindow();

  // Clipboardchange event requires a secure origin and page in focus to work.
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  auto* clipboard_change_event_handler =
      MakeGarbageCollected<EventCountingListener>();
  GetDocument().addEventListener(event_type_names::kClipboardchange,
                                 clipboard_change_event_handler, false);
  auto* clipboard_change_event_controller =
      MakeGarbageCollected<ClipboardChangeEventController>(
          *GetFrame().DomWindow()->navigator(), &GetDocument());

  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Simulate a clipboard change event from the system clipboard.
  clipboard_change_event_controller->DidUpdateData();

  // Wait for any internal callbacks to run.
  test::RunPendingTasks();

  // Expect a single clipboardchange event to be fired.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 1);

  // Clean up the event listener
  GetDocument().removeEventListener(event_type_names::kClipboardchange,
                                    clipboard_change_event_handler, false);
}

TEST_F(ClipboardChangeEventTest, ClipboardChangeEventNotFiredWhenNotFocused) {
  ExecutionContext* executionContext = GetFrame().DomWindow();

  SetSecureOrigin(executionContext);
  SetPageFocus(false);

  auto* clipboard_change_event_handler =
      MakeGarbageCollected<EventCountingListener>();
  GetDocument().addEventListener(event_type_names::kClipboardchange,
                                 clipboard_change_event_handler, false);
  auto* clipboard_change_event_controller =
      MakeGarbageCollected<ClipboardChangeEventController>(
          *GetFrame().DomWindow()->navigator(), &GetDocument());

  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Simulate a clipboard change event from the system clipboard.
  clipboard_change_event_controller->DidUpdateData();

  // Wait for any internal callbacks to run.
  test::RunPendingTasks();

  // Expect no clipboardchange event to be fired since the page is not focused.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Simulate more clipboard updates
  clipboard_change_event_controller->DidUpdateData();
  clipboard_change_event_controller->DidUpdateData();

  // Focus back and wait for any internal callbacks to run.
  SetPageFocus(true);
  test::RunPendingTasks();

  // Expect a single clipboardchange event to be fired
  // now that the page is focused.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 1);

  // Clean up the event listener
  GetDocument().removeEventListener(event_type_names::kClipboardchange,
                                    clipboard_change_event_handler, false);
}

}  // namespace blink
