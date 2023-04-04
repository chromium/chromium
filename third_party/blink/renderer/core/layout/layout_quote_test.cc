// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_quote.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutQuoteTest : public RenderingTest {
 protected:
  void CheckQuoteLayoutObjectChildrenLang(const char* id,
                                          const char* lang,
                                          const char* parent_lang) {
    const LayoutObject* o = GetLayoutObjectByElementId(id);
    EXPECT_STREQ(o->StyleRef().Locale().Ascii().c_str(), lang);

    const LayoutObject* child_before = o->SlowFirstChild();
    ASSERT_EQ(child_before->StyleRef().StyleType(), PseudoId::kPseudoIdBefore);
    EXPECT_STREQ(
        child_before->SlowFirstChild()->StyleRef().Locale().Ascii().c_str(),
        parent_lang);

    const LayoutObject* child_after = o->SlowLastChild();
    ASSERT_EQ(child_after->StyleRef().StyleType(), PseudoId::kPseudoIdAfter);
    EXPECT_STREQ(
        child_after->SlowFirstChild()->StyleRef().Locale().Ascii().c_str(),
        parent_lang);

    const LayoutObject* child_text = child_before->NextSibling();
    ASSERT_TRUE(child_text->IsText());
    EXPECT_STREQ(child_text->StyleRef().Locale().Ascii().c_str(), lang);
  }
};

// The `<q>` element delimiters should use the language from its parent.
// crbug.com/1290851
TEST_F(LayoutQuoteTest, Locale) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #en { font-weight: bold; }
    </style>
    <div id="en" lang="en">
      English
      <q id="ja" lang="ja">
        Japanese
        <q id="fr" lang="fr">
          French
        </q>
        <q id="nan">
          Nan
        </q>
      </q>
    </div>
  )HTML");

  // The "ja" element should be "ja".
  // Its `::before`/`::after` pseudo elements should be parent lang "en".
  // Its text child should be "ja".
  LayoutQuoteTest::CheckQuoteLayoutObjectChildrenLang("ja", "ja", "en");

  // The "fr" element should be "fr".
  // Its pseudo elements should be parent lang "ja".
  // Its text child should be "fr".
  LayoutQuoteTest::CheckQuoteLayoutObjectChildrenLang("fr", "fr", "ja");

  // When the lang is not defined, all lang should be dependent on parent "ja".
  LayoutQuoteTest::CheckQuoteLayoutObjectChildrenLang("nan", "ja", "ja");

  // Rendered layout object lang should persist after changes.
  // crbug.com/1366233
  To<CSSStyleSheet>(GetDocument().StyleSheets().item(0))
      ->removeRule(0, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  LayoutQuoteTest::CheckQuoteLayoutObjectChildrenLang("ja", "ja", "en");
  LayoutQuoteTest::CheckQuoteLayoutObjectChildrenLang("fr", "fr", "ja");
  LayoutQuoteTest::CheckQuoteLayoutObjectChildrenLang("nan", "ja", "ja");
}

}  // namespace blink
