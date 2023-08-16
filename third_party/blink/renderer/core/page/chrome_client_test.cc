// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/chrome_client.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class ChromeClientToolTipLogger : public EmptyChromeClient {
 public:
  void UpdateTooltipUnderCursor(LocalFrame&,
                                const String& text,
                                TextDirection) override {
    tool_tip_for_last_set_tool_tip_ = text;
  }

  String ToolTipForLastUpdateTooltipUnderCursor() const {
    return tool_tip_for_last_set_tool_tip_;
  }
  void ClearToolTipForLastUpdateTooltipUnderCursor() {
    tool_tip_for_last_set_tool_tip_ = String();
  }

 private:
  String tool_tip_for_last_set_tool_tip_;
};
}  // anonymous namespace

class ChromeClientTest : public testing::Test {};

TEST_F(ChromeClientTest, UpdateTooltipUnderCursorFlood) {
  ChromeClientToolTipLogger logger;
  ChromeClient* client = &logger;
  HitTestLocation location(PhysicalOffset(10, 20));
  HitTestResult result(HitTestRequest(HitTestRequest::kMove), location);
  ScopedNullExecutionContext execution_context;
  auto* doc = Document::CreateForTest(execution_context.GetExecutionContext());
  auto* element = MakeGarbageCollected<HTMLElement>(html_names::kDivTag, *doc);
  element->setAttribute(html_names::kTitleAttr, AtomicString("tooltip"));
  result.SetInnerNode(element);

  client->UpdateTooltipUnderCursor(*doc->GetFrame(), location, result);
  EXPECT_EQ("tooltip", logger.ToolTipForLastUpdateTooltipUnderCursor());

  // seToolTip(HitTestResult) again in the same condition.
  logger.ClearToolTipForLastUpdateTooltipUnderCursor();
  client->UpdateTooltipUnderCursor(*doc->GetFrame(), location, result);
  // UpdateTooltipUnderCursor(String,TextDirection) should not be called.
  EXPECT_EQ(String(), logger.ToolTipForLastUpdateTooltipUnderCursor());

  // Cancel the tooltip, and UpdateTooltipUnderCursor(HitTestResult) again.
  client->ClearToolTip(*doc->GetFrame());
  logger.ClearToolTipForLastUpdateTooltipUnderCursor();
  client->UpdateTooltipUnderCursor(*doc->GetFrame(), location, result);
  // UpdateTooltipUnderCursor(String,TextDirection) should not be called.
  EXPECT_EQ(String(), logger.ToolTipForLastUpdateTooltipUnderCursor());

  logger.ClearToolTipForLastUpdateTooltipUnderCursor();
  element->setAttribute(html_names::kTitleAttr, AtomicString("updated"));
  client->UpdateTooltipUnderCursor(*doc->GetFrame(), location, result);
  // UpdateTooltipUnderCursor(String,TextDirection) should be called because
  // tooltip string is different from the last one.
  EXPECT_EQ("updated", logger.ToolTipForLastUpdateTooltipUnderCursor());
}

TEST_F(ChromeClientTest, UpdateTooltipUnderCursorEmptyString) {
  ChromeClient* client = MakeGarbageCollected<EmptyChromeClient>();
  HitTestLocation location(PhysicalOffset(10, 20));
  HitTestResult result(HitTestRequest(HitTestRequest::kMove), location);
  ScopedNullExecutionContext execution_context;
  auto& doc = *Document::CreateForTest(execution_context.GetExecutionContext());
  auto& input_element = *MakeGarbageCollected<HTMLInputElement>(doc);
  input_element.setAttribute(html_names::kTypeAttr, AtomicString("file"));

  result.SetInnerNode(&input_element);
  client->UpdateTooltipUnderCursor(*doc.GetFrame(), location, result);
  EXPECT_EQ("<<NoFileChosenLabel>>", client->last_tool_tip_text_);

  client->last_tool_tip_text_ = String();
  input_element.removeAttribute(html_names::kTitleAttr);
  client->UpdateTooltipUnderCursor(*doc.GetFrame(), location, result);
  EXPECT_EQ("<<NoFileChosenLabel>>", client->last_tool_tip_text_);

  client->last_tool_tip_text_ = String();
  input_element.setAttribute(html_names::kTitleAttr, g_empty_atom);
  client->UpdateTooltipUnderCursor(*doc.GetFrame(), location, result);
  EXPECT_EQ(g_empty_atom, client->last_tool_tip_text_);

  client->last_tool_tip_text_ = String();
  input_element.setAttribute(html_names::kTitleAttr, AtomicString("test"));
  client->UpdateTooltipUnderCursor(*doc.GetFrame(), location, result);
  EXPECT_EQ("test", client->last_tool_tip_text_);
}

}  // namespace blink
