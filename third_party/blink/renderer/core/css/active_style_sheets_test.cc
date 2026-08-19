// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/active_style_sheets.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ActiveStyleSheetsTest : public PageTestBase {
 protected:
  CSSStyleSheet* CreateSheet(const String& css_text = String()) {
    auto* contents = MakeGarbageCollected<StyleSheetContents>(
        MakeGarbageCollected<CSSParserContext>(
            kHTMLStandardMode, SecureContextMode::kInsecureContext));
    contents->ParseString(css_text);
    contents->EnsureRuleSet(MediaQueryEvaluator(GetDocument().GetFrame()),
                            /*mixins=*/{});
    return MakeGarbageCollected<CSSStyleSheet>(contents);
  }
};

class ApplyRulesetsTest : public ActiveStyleSheetsTest {};

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_NoChange) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AppendedToEmpty) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsAppended,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AppendedToNonEmpty) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsAppended,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(1u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Mutated) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  sheet2->Contents()->ClearRuleSet();
  sheet2->Contents()->EnsureRuleSet(
      MediaQueryEvaluator(GetDocument().GetFrame()), /*mixins=*/{});

  EXPECT_NE(old_sheets[1].second, &sheet2->Contents()->GetRuleSet());

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(old_sheets[1].second));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Inserted) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(1u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_Removed) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(1u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RemovedAll) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(3u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_InsertedAndRemoved) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AddNullRuleSet) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(std::make_pair(sheet2, nullptr));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RemoveNullRuleSet) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(sheet2, nullptr));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AddRemoveNullRuleSet) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(sheet2, nullptr));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(std::make_pair(sheet3, nullptr));

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_RemoveNullRuleSetAndAppend) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(sheet2, nullptr));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(1u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_ReorderedSheets) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_ReorderedSheetsAfterReplacement) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();
  CSSStyleSheet* sheet4 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet4, &sheet4->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(4u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet4->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_DuplicateInsertedWithoutReorder) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));

  // Only the duplicated sheet was inserted. Sheet1 and the surviving sheet2
  // keep their relative order, so nothing else needs to be invalidated.
  EXPECT_EQ(1u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_DuplicateInsertedWithoutReorderAndWithAppend) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  // Sheet1 and the surviving occurrence of sheet2 kept their relative order,
  // so only the inserted sheet2 and sheet3 need to be invalidated.
  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_FALSE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_MixedDuplicateCountChangesAreConservative) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  // One sheet1 was removed and one sheet2 was inserted. Which occurrences
  // survived is ambiguous without finding a longest common subsequence, so
  // sheet3 is conservatively invalidated with the changed duplicate counts.
  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(3u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_ReorderedSheetsAfterCommonPrefix) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));

  // The sheet in the common prefix did not move, so it stays valid.
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RotatedSheets) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  // Rotate left. Only the first sheet moves backwards, but the cascade order
  // changes for all of them.
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(3u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_ReorderedDuplicateSheet) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  // The same sheet may legally appear more than once, so the relative order has
  // to be tracked per occurrence rather than per sheet.
  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));

  // sheet2 used to sit between the two occurrences of sheet1, and now follows
  // both of them, so the cascade order changed even though the set of sheets
  // and their rule sets did not.
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_DuplicateSheetRemoved) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* old_leading_sheet = CreateSheet();
  CSSStyleSheet* new_leading_sheet = CreateSheet();
  CSSStyleSheet* duplicated_sheet = CreateSheet();
  CSSStyleSheet* trailing_sheet = CreateSheet();

  old_sheets.push_back(std::make_pair(
      old_leading_sheet, &old_leading_sheet->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(
      duplicated_sheet, &duplicated_sheet->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(
      duplicated_sheet, &duplicated_sheet->Contents()->GetRuleSet()));
  old_sheets.push_back(std::make_pair(
      trailing_sheet, &trailing_sheet->Contents()->GetRuleSet()));

  new_sheets.push_back(std::make_pair(
      new_leading_sheet, &new_leading_sheet->Contents()->GetRuleSet()));
  new_sheets.push_back(std::make_pair(
      duplicated_sheet, &duplicated_sheet->Contents()->GetRuleSet()));
  new_sheets.push_back(std::make_pair(
      trailing_sheet, &trailing_sheet->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(3u, changed_rule_sets.size());
  EXPECT_TRUE(
      changed_rule_sets.Contains(&old_leading_sheet->Contents()->GetRuleSet()));
  EXPECT_TRUE(
      changed_rule_sets.Contains(&new_leading_sheet->Contents()->GetRuleSet()));
  EXPECT_TRUE(
      changed_rule_sets.Contains(&duplicated_sheet->Contents()->GetRuleSet()));
  EXPECT_FALSE(
      changed_rule_sets.Contains(&trailing_sheet->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_MutatedAndReordered) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* mutated_sheet = CreateSheet();
  CSSStyleSheet* moved_sheet = CreateSheet();

  old_sheets.push_back(
      std::make_pair(mutated_sheet, &mutated_sheet->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(moved_sheet, &moved_sheet->Contents()->GetRuleSet()));

  mutated_sheet->Contents()->ClearRuleSet();
  mutated_sheet->Contents()->EnsureRuleSet(
      MediaQueryEvaluator(GetDocument().GetFrame()), /*mixins=*/{});
  EXPECT_NE(old_sheets[0].second, &mutated_sheet->Contents()->GetRuleSet());

  new_sheets.push_back(
      std::make_pair(moved_sheet, &moved_sheet->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(mutated_sheet, &mutated_sheet->Contents()->GetRuleSet()));

  // The moved sheet only changed its cascade order relative to the mutated
  // sheet, and every element which that can affect matches a rule in the
  // mutated sheet, whose old and new rule sets are both invalidated below.
  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(old_sheets[0].second));
  EXPECT_TRUE(
      changed_rule_sets.Contains(&mutated_sheet->Contents()->GetRuleSet()));
  EXPECT_FALSE(
      changed_rule_sets.Contains(&moved_sheet->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_ReorderedAndRemoved) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(3u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest,
       CompareActiveStyleSheets_ReorderedBetweenUnchanged) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();
  CSSStyleSheet* sheet4 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet4, &sheet4->Contents()->GetRuleSet()));

  new_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet4, &sheet4->Contents()->GetRuleSet()));

  // Only the two sheets which swapped need to be invalidated. The sheets
  // before and after them kept both their rules and their relative order, so
  // no cascade which involves them can have changed.
  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet2->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
  EXPECT_FALSE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_FALSE(changed_rule_sets.Contains(&sheet4->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_RemovedWithoutReorder) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();
  CSSStyleSheet* sheet3 = CreateSheet();
  CSSStyleSheet* sheet4 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet3, &sheet3->Contents()->GetRuleSet()));
  old_sheets.push_back(
      std::make_pair(sheet4, &sheet4->Contents()->GetRuleSet()));

  // Removing sheets keeps the relative order of the survivors, so only the
  // removed rule sets are invalidated.
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));
  new_sheets.push_back(
      std::make_pair(sheet4, &sheet4->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet1->Contents()->GetRuleSet()));
  EXPECT_TRUE(changed_rule_sets.Contains(&sheet3->Contents()->GetRuleSet()));
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_ReorderedNullRuleSets) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(std::make_pair(sheet1, nullptr));
  old_sheets.push_back(std::make_pair(sheet2, nullptr));

  new_sheets.push_back(std::make_pair(sheet2, nullptr));
  new_sheets.push_back(std::make_pair(sheet1, nullptr));

  // Sheets which contribute no rules cannot change the cascade by moving.
  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_DisableAndAppend) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  CSSStyleSheet* sheet1 = CreateSheet();
  CSSStyleSheet* sheet2 = CreateSheet();

  old_sheets.push_back(
      std::make_pair(sheet1, &sheet1->Contents()->GetRuleSet()));
  new_sheets.push_back(std::make_pair(sheet1, nullptr));
  new_sheets.push_back(
      std::make_pair(sheet2, &sheet2->Contents()->GetRuleSet()));

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(2u, changed_rule_sets.size());
}

TEST_F(ActiveStyleSheetsTest, CompareActiveStyleSheets_AddRemoveNonMatchingMQ) {
  ActiveStyleSheetVector old_sheets;
  ActiveStyleSheetVector new_sheets;
  HeapHashSet<Member<RuleSet>> changed_rule_sets;

  EXPECT_EQ(
      kNoActiveSheetsChanged,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());

  CSSStyleSheet* sheet1 = CreateSheet();
  MediaQuerySet* mq =
      MediaQueryParser::ParseMediaQuerySet("(min-width: 9000px)", nullptr);
  sheet1->SetMediaQueries(mq);
  sheet1->MatchesMediaQueries(MediaQueryEvaluator(GetDocument().GetFrame()));

  new_sheets.push_back(std::make_pair(sheet1, nullptr));

  EXPECT_EQ(
      kActiveSheetsAppended,
      CompareActiveStyleSheets(old_sheets, new_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());

  EXPECT_EQ(
      kActiveSheetsChanged,
      CompareActiveStyleSheets(new_sheets, old_sheets, {}, changed_rule_sets));
  EXPECT_EQ(0u, changed_rule_sets.size());
}

TEST_F(ApplyRulesetsTest, AddUniversalRuleToDocument) {
  UpdateAllLifecyclePhasesForTest();

  CSSStyleSheet* sheet = CreateSheet("body * { color:red }");

  ActiveStyleSheetVector new_style_sheets;
  new_style_sheets.push_back(
      std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));

  GetStyleEngine().ApplyRuleSetChanges(GetDocument(), ActiveStyleSheetVector(),
                                       new_style_sheets, {});

  EXPECT_FALSE(GetStyleEngine().NeedsStyleInvalidation());
  EXPECT_FALSE(GetStyleEngine().NeedsStyleRecalc());
}

TEST_F(ApplyRulesetsTest, AddUniversalRuleToShadowTree) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<div id=host></div>");
  Element* host = GetElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhasesForTest();

  CSSStyleSheet* sheet = CreateSheet("body * { color:red }");

  ActiveStyleSheetVector new_style_sheets;
  new_style_sheets.push_back(
      std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));

  GetStyleEngine().ApplyRuleSetChanges(shadow_root, ActiveStyleSheetVector(),
                                       new_style_sheets, {});

  EXPECT_FALSE(GetStyleEngine().NeedsStyleInvalidation());
  EXPECT_FALSE(GetStyleEngine().NeedsStyleRecalc());
}

TEST_F(ApplyRulesetsTest, AddFontFaceRuleToDocument) {
  UpdateAllLifecyclePhasesForTest();

  CSSStyleSheet* sheet =
      CreateSheet("@font-face { font-family: ahum; src: url(ahum.ttf) }");

  ActiveStyleSheetVector new_style_sheets;
  new_style_sheets.push_back(
      std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));

  GetStyleEngine().ApplyRuleSetChanges(GetDocument(), ActiveStyleSheetVector(),
                                       new_style_sheets, {});

  EXPECT_EQ(kNoStyleChange,
            GetDocument().documentElement()->GetStyleChangeType());
}

TEST_F(ApplyRulesetsTest, AddFontFaceRuleToShadowTree) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<div id=host></div>");
  Element* host = GetElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  UpdateAllLifecyclePhasesForTest();

  CSSStyleSheet* sheet =
      CreateSheet("@font-face { font-family: ahum; src: url(ahum.ttf) }");

  ActiveStyleSheetVector new_style_sheets;
  new_style_sheets.push_back(
      std::make_pair(sheet, &sheet->Contents()->GetRuleSet()));

  GetStyleEngine().ApplyRuleSetChanges(shadow_root, ActiveStyleSheetVector(),
                                       new_style_sheets, {});

  EXPECT_FALSE(GetDocument().NeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleRecalc());
  EXPECT_FALSE(GetDocument().NeedsStyleInvalidation());
  EXPECT_FALSE(GetDocument().ChildNeedsStyleInvalidation());
}

TEST_F(ApplyRulesetsTest, RemoveSheetFromShadowTree) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<div id=host></div>");
  Element* host = GetElementById("host");
  ASSERT_TRUE(host);

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<style>::slotted(#dummy){color:pink}</style>");
  UpdateAllLifecyclePhasesForTest();

  ASSERT_EQ(1u, shadow_root.StyleSheets().length());

  StyleSheet* sheet = shadow_root.StyleSheets().item(0);
  ASSERT_TRUE(sheet);
  ASSERT_TRUE(sheet->IsCSSStyleSheet());

  auto* css_sheet = To<CSSStyleSheet>(sheet);
  ActiveStyleSheetVector old_style_sheets;
  old_style_sheets.push_back(
      std::make_pair(css_sheet, &css_sheet->Contents()->GetRuleSet()));
  GetStyleEngine().ApplyRuleSetChanges(shadow_root, old_style_sheets,
                                       ActiveStyleSheetVector(), {});
}

}  // namespace blink
