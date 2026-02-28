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
  EXPECT_TRUE(messages[0].contains("focusgroup requires a behavior token"));
  EXPECT_TRUE(messages[0].contains("first value"));
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
  EXPECT_TRUE(messages[0].contains("focusgroup requires a behavior token"));
  EXPECT_TRUE(messages[0].contains("Found: 'invalid'"));
}

TEST_F(FocusgroupFlagsTest, MultipleBehaviorTokensGenerateWarning) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar tablist"));

  // Current implementation: toolbar is parsed as behavior, tablist as invalid
  // modifier.  Toolbar defaults to inline-only axis.
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline);

  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].contains("Unrecognized focusgroup attribute values"));
  EXPECT_TRUE(messages[0].contains("tablist"));
}

TEST_F(FocusgroupFlagsTest, UnknownTokenGeneratesWarning) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar unknown"));
  // Toolbar defaults to inline-only axis.
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline);
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].contains("Unrecognized focusgroup attribute values"));
  EXPECT_TRUE(messages[0].contains("unknown"));
  EXPECT_TRUE(messages[0].contains("Valid tokens are"));
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
  EXPECT_TRUE(messages[0].contains("disables focusgroup behavior"));
  EXPECT_TRUE(messages[0].contains("all other tokens are ignored"));
}

TEST_F(FocusgroupFlagsTest, RedundantInlineBlockGeneratesWarning) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Redundancy warning only fires for behaviors without a default axis.
  // Radiogroup has no default axis, so explicit inline+block is redundant.
  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("radiogroup inline block"));

  EXPECT_EQ(result.behavior, FocusgroupBehavior::kRadiogroup);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline | FocusgroupFlags::kBlock);
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].contains("redundant"));
  EXPECT_TRUE(messages[0].contains("default behavior for linear focusgroups"));

  // Toolbar has a default axis (inline), so inline+block is NOT redundant;
  // it is an explicit override adding the block axis.
  ClearConsoleMessages();
  FocusgroupData toolbar_result =
      ParseFocusgroup(element, AtomicString("toolbar inline block"));

  EXPECT_EQ(toolbar_result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(toolbar_result.flags,
            FocusgroupFlags::kInline | FocusgroupFlags::kBlock);
  auto toolbar_messages = CopyConsoleMessages();
  EXPECT_EQ(toolbar_messages.size(), 0u);
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
  EXPECT_TRUE(messages[0].contains("not valid for grid focusgroups"));
  EXPECT_TRUE(messages[0].contains("row-wrap/col-wrap or flow modifiers"));
}

TEST_F(FocusgroupFlagsTest, GridTokensOnLinearGenerateError) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar row-wrap"));

  // Linear focusgroup uses toolbar's default axis (inline); row-wrap ignored
  // with warning.
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline);
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].contains("only valid for grid focusgroups"));
  EXPECT_TRUE(messages[0].contains("use 'wrap' for linear focusgroups"));
}

TEST_F(FocusgroupFlagsTest, NowrapAlone) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar nowrap"));

  // Toolbar defaults to inline axis; nowrap suppresses any wrap.
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline);
  EXPECT_EQ(CopyConsoleMessages().size(), 0u);
}

TEST_F(FocusgroupFlagsTest, WrapAndNowrapConflict) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result =
      ParseFocusgroup(element, AtomicString("toolbar wrap nowrap"));

  // wrap + nowrap is an author error; both are ignored.
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline);
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].contains("author error"));
}

TEST_F(FocusgroupFlagsTest, NowrapOnGrid) {
  ScopedFocusgroupForTest focusgroup_scope(true);
  ScopedFocusgroupGridForTest grid_scope(true);

  auto* element = MakeGarbageCollected<HTMLTableElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("grid nowrap"));

  EXPECT_EQ(result.behavior, FocusgroupBehavior::kGrid);
  EXPECT_EQ(result.flags, FocusgroupFlags::kNone);
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);
  EXPECT_TRUE(messages[0].contains("nowrap"));
  EXPECT_TRUE(messages[0].contains("not valid for grid"));
}

TEST_F(FocusgroupFlagsTest, NowrapOverridesDefaultWrap) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Tablist defaults to inline + wrap.  nowrap suppresses the default wrap.
  ClearConsoleMessages();
  FocusgroupData tablist_result =
      ParseFocusgroup(element, AtomicString("tablist nowrap"));
  EXPECT_EQ(tablist_result.behavior, FocusgroupBehavior::kTablist);
  EXPECT_EQ(tablist_result.flags, FocusgroupFlags::kInline);

  // Menu defaults to block + wrap.  nowrap suppresses the default wrap.
  ClearConsoleMessages();
  FocusgroupData menu_result =
      ParseFocusgroup(element, AtomicString("menu nowrap"));
  EXPECT_EQ(menu_result.behavior, FocusgroupBehavior::kMenu);
  EXPECT_EQ(menu_result.flags, FocusgroupFlags::kBlock);
}

TEST_F(FocusgroupFlagsTest, ToolbarDefaults) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Bare toolbar defaults to inline only, no wrap.
  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("toolbar"));
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline);

  // Explicit block overrides default inline.
  ClearConsoleMessages();
  FocusgroupData block_result =
      ParseFocusgroup(element, AtomicString("toolbar block"));
  EXPECT_EQ(block_result.behavior, FocusgroupBehavior::kToolbar);
  EXPECT_EQ(block_result.flags, FocusgroupFlags::kBlock);
}

TEST_F(FocusgroupFlagsTest, TablistDefaults) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Bare tablist defaults to inline + wrap-inline.
  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("tablist"));
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kTablist);
  EXPECT_EQ(result.flags,
            FocusgroupFlags::kInline | FocusgroupFlags::kWrapInline);

  // Explicit block overrides default inline; default wrap applies in block.
  ClearConsoleMessages();
  FocusgroupData block_result =
      ParseFocusgroup(element, AtomicString("tablist block"));
  EXPECT_EQ(block_result.behavior, FocusgroupBehavior::kTablist);
  EXPECT_EQ(block_result.flags,
            FocusgroupFlags::kBlock | FocusgroupFlags::kWrapBlock);

  // nowrap suppresses default wrap.
  ClearConsoleMessages();
  FocusgroupData nowrap_result =
      ParseFocusgroup(element, AtomicString("tablist nowrap"));
  EXPECT_EQ(nowrap_result.behavior, FocusgroupBehavior::kTablist);
  EXPECT_EQ(nowrap_result.flags, FocusgroupFlags::kInline);
}

TEST_F(FocusgroupFlagsTest, MenuDefaults) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Bare menu defaults to block + wrap-block.
  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("menu"));
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kMenu);
  EXPECT_EQ(result.flags,
            FocusgroupFlags::kBlock | FocusgroupFlags::kWrapBlock);

  // Explicit inline overrides default block; default wrap applies in inline.
  ClearConsoleMessages();
  FocusgroupData inline_result =
      ParseFocusgroup(element, AtomicString("menu inline"));
  EXPECT_EQ(inline_result.behavior, FocusgroupBehavior::kMenu);
  EXPECT_EQ(inline_result.flags,
            FocusgroupFlags::kInline | FocusgroupFlags::kWrapInline);
}

TEST_F(FocusgroupFlagsTest, MenubarDefaults) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Bare menubar defaults to inline + wrap-inline.
  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("menubar"));
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kMenubar);
  EXPECT_EQ(result.flags,
            FocusgroupFlags::kInline | FocusgroupFlags::kWrapInline);

  // Explicit block overrides default inline; default wrap applies in block.
  ClearConsoleMessages();
  FocusgroupData block_result =
      ParseFocusgroup(element, AtomicString("menubar block"));
  EXPECT_EQ(block_result.behavior, FocusgroupBehavior::kMenubar);
  EXPECT_EQ(block_result.flags,
            FocusgroupFlags::kBlock | FocusgroupFlags::kWrapBlock);
}

TEST_F(FocusgroupFlagsTest, RadiogroupNoDefaults) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Radiogroup has no defaults; both axes, no wrap.
  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("radiogroup"));
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kRadiogroup);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline | FocusgroupFlags::kBlock);
}

TEST_F(FocusgroupFlagsTest, ListboxNoDefaults) {
  ScopedFocusgroupForTest focusgroup_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Listbox has no defaults; both axes, no wrap.
  ClearConsoleMessages();
  FocusgroupData result = ParseFocusgroup(element, AtomicString("listbox"));
  EXPECT_EQ(result.behavior, FocusgroupBehavior::kListbox);
  EXPECT_EQ(result.flags, FocusgroupFlags::kInline | FocusgroupFlags::kBlock);
}

TEST_F(FocusgroupFlagsTest, ValidTokenListStringIncludesNowrap) {
  ScopedFocusgroupForTest focusgroup_scope(true);
  ScopedFocusgroupGridForTest grid_scope(false);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // Trigger an unrecognized-token warning so the console message includes the
  // full valid token list produced by ValidTokenListString().
  ClearConsoleMessages();
  ParseFocusgroup(element, AtomicString("toolbar bogus"));
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);

  // Without the grid feature, grid-only tokens are filtered out. The expected
  // list is: inline, block, wrap, no-memory, nowrap.
  EXPECT_TRUE(messages[0].contains("inline"));
  EXPECT_TRUE(messages[0].contains("block"));
  EXPECT_TRUE(messages[0].contains("wrap"));
  EXPECT_TRUE(messages[0].contains("no-memory"));
  EXPECT_TRUE(messages[0].contains("nowrap"));

  // Grid-only tokens must not appear.
  EXPECT_FALSE(messages[0].contains("row-wrap"));
  EXPECT_FALSE(messages[0].contains("col-wrap"));
  EXPECT_FALSE(messages[0].contains("row-flow"));
  EXPECT_FALSE(messages[0].contains("col-flow"));
}

TEST_F(FocusgroupFlagsTest, ValidTokenListStringIncludesGridTokens) {
  ScopedFocusgroupForTest focusgroup_scope(true);
  ScopedFocusgroupGridForTest grid_scope(true);

  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  GetDocument().body()->appendChild(element);

  // With grid enabled, use a grid behavior so grid-only tokens are not warned
  // individually. Trigger unrecognized-token warning for the full list.
  ClearConsoleMessages();
  ParseFocusgroup(element, AtomicString("toolbar bogus"));
  auto messages = CopyConsoleMessages();
  ASSERT_GE(messages.size(), 1u);

  // All tokens including grid-only should appear.
  EXPECT_TRUE(messages[0].contains("row-wrap"));
  EXPECT_TRUE(messages[0].contains("col-wrap"));
  EXPECT_TRUE(messages[0].contains("flow"));
  EXPECT_TRUE(messages[0].contains("row-flow"));
  EXPECT_TRUE(messages[0].contains("col-flow"));
  EXPECT_TRUE(messages[0].contains("nowrap"));
}

}  // namespace blink::focusgroup
