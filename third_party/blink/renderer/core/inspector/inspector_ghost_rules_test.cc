// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_ghost_rules.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_nested_declarations_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class InspectorGhostRuleTest : public testing::Test,
                               public testing::WithParamInterface<const char*> {
 protected:
  void SetUp() override;

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  template <typename T>
  void SerializeChildren(T* container, StringBuilder& sb) {
    for (wtf_size_t i = 0; i < container->length(); ++i) {
      Serialize(container->ItemInternal(i), sb);
    }
  }

  void Serialize(CSSRule* rule, StringBuilder& sb) {
    // Provide nicer debugging output for some types we care about.
    if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
      sb.Append(style_rule->selectorText());
    } else if (auto* media_rule = DynamicTo<CSSMediaRule>(rule)) {
      sb.Append("@media ");
      sb.Append(media_rule->conditionText());
    } else if (auto* nested_declarations_rule =
                   DynamicTo<CSSNestedDeclarationsRule>(rule)) {
      Serialize(nested_declarations_rule->InnerCSSStyleRule(), sb);
      return;
    } else {
      sb.AppendNumber(static_cast<int>(rule->GetType()));
    }

    sb.Append(" { ");

    if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
      sb.Append(style_rule->style()->cssText());
      sb.Append(" ");
      SerializeChildren(style_rule, sb);
    } else if (auto* grouping_rule = DynamicTo<CSSGroupingRule>(rule)) {
      SerializeChildren(grouping_rule, sb);
    }

    sb.Append(" } ");
  }

  String Serialize(CSSStyleSheet* sheet) {
    StringBuilder sb;
    SerializeChildren(sheet, sb);
    return sb.ToString();
  }

  void RemoveGhostDeclarations(CSSRule* rule) {
    if (auto* nested_declarations_rule =
            DynamicTo<CSSNestedDeclarationsRule>(rule)) {
      To<CSSStyleRule>(nested_declarations_rule->InnerCSSStyleRule())
          ->style()
          ->removeProperty("--ghost", ASSERT_NO_EXCEPTION);
    } else if (auto* grouping_rule = DynamicTo<CSSGroupingRule>(rule)) {
      for (wtf_size_t i = 0; i < grouping_rule->length(); ++i) {
        RemoveGhostDeclarations(grouping_rule->ItemInternal(i));
      }
    } else if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
      for (wtf_size_t i = 0; i < style_rule->length(); ++i) {
        RemoveGhostDeclarations(style_rule->ItemInternal(i));
      }
    }
  }

  void RemoveGhostDeclarations(CSSStyleSheet* sheet) {
    for (wtf_size_t i = 0; i < sheet->length(); ++i) {
      RemoveGhostDeclarations(sheet->ItemInternal(i));
    }
  }

  CSSStyleSheet* CreateExpectedSheet(String expected_text) {
    CSSStyleSheet* expected_sheet =
        css_test_helpers::CreateStyleSheet(GetDocument());
    expected_sheet->SetText(expected_text, CSSImportRules::kIgnoreWithWarning);
    // Remove any '--ghost' declarations, leaving empty CSSNestedDeclarations
    // behind. (See `ghost_rules_data`).
    RemoveGhostDeclarations(expected_sheet);
    return expected_sheet;
  }

  CSSStyleSheet* CreateActualSheet(String actual_text) {
    CSSStyleSheet* actual_sheet =
        css_test_helpers::CreateStyleSheet(GetDocument());
    // InspectorGhostRules should create the ghost rules (i.e. empty
    // CSSNestedDeclarations).
    actual_text.Replace("--ghost: 1;", "");  // See `ghost_rules_data`.
    actual_sheet->SetText(actual_text, CSSImportRules::kIgnoreWithWarning);
    return actual_sheet;
  }

  void Populate(InspectorGhostRules& ghost_rules, CSSStyleSheet& sheet) {
    ghost_rules.Populate(sheet);
  }

 private:
  test::TaskEnvironment task_environment_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void InspectorGhostRuleTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
}

TEST_P(InspectorGhostRuleTest, RefTest) {
  String actual_text(GetParam());
  String expected_text(GetParam());

  SCOPED_TRACE(testing::Message() << "Actual input text: " << actual_text);
  SCOPED_TRACE(testing::Message() << "Expected input text: " << expected_text);

  CSSStyleSheet* expected_sheet = CreateExpectedSheet(expected_text);
  CSSStyleSheet* actual_sheet = CreateActualSheet(actual_text);

  String before_string = Serialize(actual_sheet);

  {
    InspectorGhostRules ghost_rules;
    Populate(ghost_rules, *actual_sheet);
    EXPECT_EQ(Serialize(expected_sheet), Serialize(actual_sheet));
  }

  // When InspectorGhostRules goes out of scope, `actual_sheet` should go back
  // to normal.
  String after_string = Serialize(actual_sheet);
  EXPECT_EQ(before_string, after_string);
}

TEST_P(InspectorGhostRuleTest, SharedContents) {
  String actual_text(GetParam());
  String expected_text(GetParam());

  SCOPED_TRACE(testing::Message() << "Actual input text: " << actual_text);
  SCOPED_TRACE(testing::Message() << "Expected input text: " << expected_text);

  CSSStyleSheet* expected_sheet = CreateExpectedSheet(expected_text);
  CSSStyleSheet* actual_sheet = CreateActualSheet(actual_text);

  // Share contents with actual_sheet.
  CSSStyleSheet* second_sheet =
      MakeGarbageCollected<CSSStyleSheet>(actual_sheet->Contents());
  // Pretend it's a cache hit.
  second_sheet->Contents()->SetIsUsedFromTextCache();

  StyleSheetContents* original_contents = actual_sheet->Contents();
  ASSERT_EQ(second_sheet->Contents(), original_contents);

  String before_string = Serialize(actual_sheet);
  ASSERT_EQ(Serialize(second_sheet), before_string);

  {
    InspectorGhostRules ghost_rules;
    Populate(ghost_rules, *actual_sheet);

    EXPECT_EQ(Serialize(actual_sheet), Serialize(expected_sheet));

    // `second_sheet` must remain unchanged, despite (originally) sharing
    // contents with `actual_sheet`.
    EXPECT_EQ(second_sheet->Contents(), original_contents);
    EXPECT_EQ(Serialize(second_sheet), before_string);
  }

  EXPECT_EQ(actual_sheet->Contents(), original_contents);
  EXPECT_EQ(second_sheet->Contents(), original_contents);
}

// crbug.com/417619104
TEST_P(InspectorGhostRuleTest, IncorrectlySharedContents) {
  String actual_text(GetParam());
  String expected_text(GetParam());

  SCOPED_TRACE(testing::Message() << "Actual input text: " << actual_text);
  SCOPED_TRACE(testing::Message() << "Expected input text: " << expected_text);

  CSSStyleSheet* expected_sheet = CreateExpectedSheet(expected_text);

  CSSStyleSheet* actual1 = CreateActualSheet(actual_text);
  CSSStyleSheet* actual2 = CreateActualSheet(actual_text);
  CSSStyleSheet* actual3 = CreateActualSheet(actual_text);

  // Nothing shares contents with actual1.

  // actual2 has proper sharing:
  CSSStyleSheet* actual2b =
      MakeGarbageCollected<CSSStyleSheet>(actual2->Contents(), GetDocument());
  // Pretend it's a cache hit.
  actual2b->Contents()->SetIsUsedFromTextCache();

  // actual3 has invalid sharing:
  CSSStyleSheet* actual3b =
      MakeGarbageCollected<CSSStyleSheet>(actual3->Contents(), GetDocument());
  // No call to SetIsUsedFromTextCache here.

  StyleSheetContents* original_contents1 = actual1->Contents();
  StyleSheetContents* original_contents2 = actual2->Contents();
  StyleSheetContents* original_contents3 = actual3->Contents();

  ASSERT_NE(original_contents1, original_contents2);
  ASSERT_NE(original_contents2, original_contents3);
  ASSERT_EQ(actual2b->Contents(), original_contents2);
  ASSERT_EQ(actual3b->Contents(), original_contents3);

  String before_string = Serialize(actual2);
  ASSERT_EQ(Serialize(actual2b), before_string);
  ASSERT_EQ(Serialize(actual3b), before_string);

  {
    InspectorGhostRules ghost_rules;

    HeapVector<Member<CSSStyleSheet>> sheets;
    // In "random" order:
    sheets.push_back(actual2b);
    sheets.push_back(actual1);
    sheets.push_back(actual3b);
    sheets.push_back(actual2);
    sheets.push_back(actual3);

    bool success = ghost_rules.PopulateSheets(std::move(sheets));
    EXPECT_FALSE(success);

    // Population should have succeeded for actual1:
    EXPECT_EQ(Serialize(actual1), Serialize(expected_sheet));

    // Population should have succeeded for actual2[b]:
    EXPECT_EQ(Serialize(actual2), Serialize(expected_sheet));
    EXPECT_EQ(Serialize(actual2b), Serialize(expected_sheet));

    // actual3[b] should be untouched due to illegal sharing:
    EXPECT_EQ(Serialize(actual3), before_string);
    EXPECT_EQ(Serialize(actual3b), before_string);
  }

  EXPECT_EQ(actual1->Contents(), original_contents1);
  EXPECT_EQ(actual2->Contents(), original_contents2);
  EXPECT_EQ(actual2b->Contents(), original_contents2);
  EXPECT_EQ(actual3->Contents(), original_contents3);
  EXPECT_EQ(actual3b->Contents(), original_contents3);
}

// For each of the items in this array, we'll produce an 'actual' stylesheet
// and an 'expected' stylesheet, and see if they are the same.
// The 'actual' stylesheet will be modified by InspectorGhostRules,
// and the 'expected' stylesheet will not.
//
// To indicate where ghost rules are expected, use '--ghost: 1;'.
const char* ghost_rules_data[] = {
    // Top-level rules are not affected by InspectorGhostRules.
    R"CSS(
      .a { }
      .b { }
      .c { }
      @media (width > 100px) { }
    )CSS",

    // No ghost rules should be inserted for a style rule that just contains
    // declarations.
    R"CSS(
      .a {
        color: red;
        left: 100px;
      }
    )CSS",

    R"CSS(
      .a {
        color: red;
        .b {}
        --ghost: 1;
      }
    )CSS",

    R"CSS(
      .a {
        color: red;
        .b {}
        --ghost: 1;
        .c {}
        --ghost: 1;
      }
    )CSS",

    R"CSS(
      .a {
        color: red;
        .b {}
        left: 100px;
        .c {}
        --ghost: 1;
      }
    )CSS",

    R"CSS(
      .a {
        color: red;
        .b {}
        --ghost: 1;
        .c {}
        right: 100px;
      }
    )CSS",

    R"CSS(
      .a {
        @media (width > 100px) {
          --ghost: 1;
        }
        --ghost: 1;
      }
    )CSS",

    R"CSS(
      .a {
        @media (width > 100px) {
          color: red;
        }
        --ghost: 1;
      }
    )CSS",

    R"CSS(
      .a {
        @media (width > 100px) {
          --ghost: 1;
        }
        color: red;
      }
    )CSS",

    R"CSS(
      .a {
        @media (width > 100px) {
          --ghost: 1;
        }
        --ghost: 1;
        @media (width > 200px) {
          --ghost: 1;
        }
        color: red;
        @media (width > 300px) {
          --ghost: 1;
        }
        --ghost: 1;
      }
    )CSS",
};

INSTANTIATE_TEST_SUITE_P(All,
                         InspectorGhostRuleTest,
                         testing::ValuesIn(ghost_rules_data));

}  // namespace blink
