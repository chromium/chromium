// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink::focusgroup {

class FocusgroupFlagsTest : public SimTest {
 protected:
  void SetUp() override { SimTest::SetUp(); }

  Vector<String>& RawConsoleMessages() { return ConsoleMessages(); }

  void ClearConsoleMessages() { RawConsoleMessages().clear(); }

  Vector<String> CopyConsoleMessages() { return RawConsoleMessages(); }
};

TEST_F(FocusgroupFlagsTest, EmptyAttributeGeneratesError) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString(""));

  EXPECT_EQ(result.behavior, FocusgroupBehavior::kNoBehavior);
  EXPECT_EQ(result.flags, FocusgroupFlags::kNone);

  auto messages = CopyConsoleMessages();
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_TRUE(messages[0].Contains("focusgroup requires a behavior token"));
  EXPECT_TRUE(messages[0].Contains("first value"));
}

TEST_F(FocusgroupFlagsTest, InvalidFirstTokenGeneratesError) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("invalid"));

  EXPECT_EQ(result.behavior, FocusgroupBehavior::kNoBehavior);
  EXPECT_EQ(result.flags, FocusgroupFlags::kNone);

  auto messages = CopyConsoleMessages();
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_TRUE(messages[0].Contains("focusgroup requires a behavior token"));
  EXPECT_TRUE(messages[0].Contains("Found: 'invalid'"));
}

TEST_F(FocusgroupFlagsTest, MultipleBehaviorTokensGenerateWarning) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar tablist"));

  // Current implementation: toolbar is parsed as behavior, tablist as invalid
  // modifier
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, (FocusgroupFlags::kInline | FocusgroupFlags::kBlock));

  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].Contains("Unrecognized focusgroup attribute values"));
  EXPECT_TRUE(messages[0].Contains("tablist"));
}

TEST_F(FocusgroupFlagsTest, UnknownTokenGeneratesWarning) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar unknown"));
  // Default axes (inline|block) are added for linear focusgroups when not
  // specified.
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, (FocusgroupFlags::kInline | FocusgroupFlags::kBlock));
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].Contains("Unrecognized focusgroup attribute values"));
  EXPECT_TRUE(messages[0].Contains("unknown"));
  EXPECT_TRUE(messages[0].Contains("Valid tokens are"));
}

TEST_F(FocusgroupFlagsTest, NoneWithOtherTokensGeneratesWarning) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("none inline"));

  EXPECT_EQ(result.behavior, FocusgroupBehavior::kOptOut);
  EXPECT_EQ(result.flags, FocusgroupFlags::kNone);

  auto messages = CopyConsoleMessages();
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_TRUE(messages[0].Contains("disables focusgroup behavior"));
  EXPECT_TRUE(messages[0].Contains("all other tokens are ignored"));
}

TEST_F(FocusgroupFlagsTest, RedundantInlineBlockGeneratesWarning) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar inline block"));

  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline | FocusgroupFlags::kBlock);
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].Contains("redundant"));
  EXPECT_TRUE(messages[0].Contains("default behavior for linear focusgroups"));
}

TEST_F(FocusgroupFlagsTest, InvalidAxisForGridGeneratesError) {
  ScopedFocusgroupForTest focusgroup_scope(true);
  ScopedFocusgroupGridForTest grid_scope(true);

  auto* element = MakeGarbageCollected<HTMLTableElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("grid inline"));

  EXPECT_EQ(result.behavior, FocusgroupBehavior::kGrid);
  EXPECT_EQ(result.flags, FocusgroupFlags::kNone);

  auto messages = CopyConsoleMessages();
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_TRUE(messages[0].Contains("not valid for grid focusgroups"));
  EXPECT_TRUE(messages[0].Contains("row-wrap/col-wrap or flow modifiers"));
}

TEST_F(FocusgroupFlagsTest, GridTokensOnLinearGenerateError) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar row-wrap"));

  // Linear focusgroup adds default axes; row-wrap ignored with error.
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, (FocusgroupFlags::kInline | FocusgroupFlags::kBlock));
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].Contains("only valid for grid focusgroups"));
  EXPECT_TRUE(messages[0].Contains("use 'wrap' for linear focusgroups"));
}

}  // namespace blink::focusgroup
