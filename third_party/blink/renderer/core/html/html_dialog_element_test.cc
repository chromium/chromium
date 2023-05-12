// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_dialog_element.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLDialogElementTest : public PageTestBase {};

// The dialog event should not be closed in response to cancel events.
TEST_F(HTMLDialogElementTest, CancelEventDontClose) {
  auto* dialog = MakeGarbageCollected<HTMLDialogElement>(GetDocument());
  GetDocument().FirstBodyElement()->AppendChild(dialog);
  dialog->showModal(ASSERT_NO_EXCEPTION);
  dialog->DispatchScopedEvent(*Event::CreateBubble(event_type_names::kCancel));
  EXPECT_TRUE(dialog->FastHasAttribute(html_names::kOpenAttr));
}

}  // namespace blink
