// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/scoped_css_value.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StyleBuilderTest : public PageTestBase {};

TEST_F(StyleBuilderTest, WritingModeChangeDirtiesFont) {
  const CSSProperty* properties[] = {
      &GetCSSPropertyWritingMode(),
      &GetCSSPropertyWebkitWritingMode(),
  };

  HeapVector<Member<const CSSValue>> values = {
      CSSInitialValue::Create(),
      CSSInheritedValue::Create(),
      CSSIdentifierValue::Create(CSSValueID::kHorizontalTb),
  };

  for (const CSSProperty* property : properties) {
    for (const CSSValue* value : values) {
      auto parent_style =
          GetDocument().GetStyleResolver().CreateComputedStyle();
      StyleResolverState state(GetDocument(), *GetDocument().body(),
                               nullptr /* StyleRecalcContext */,
                               StyleRequest(parent_style.get()));
      state.SetStyle(GetDocument().GetStyleResolver().CreateComputedStyle());

      // This test assumes that initial 'writing-mode' is not 'vertical-lr'.
      ASSERT_NE(WritingMode::kVerticalLr,
                state.StyleBuilder().GetWritingMode());
      state.StyleBuilder().SetWritingMode(WritingMode::kVerticalLr);

      ASSERT_FALSE(state.GetFontBuilder().FontDirty());
      StyleBuilder::ApplyProperty(*property, state,
                                  ScopedCSSValue(*value, &GetDocument()));
      EXPECT_TRUE(state.GetFontBuilder().FontDirty());
    }
  }
}

TEST_F(StyleBuilderTest, TextOrientationChangeDirtiesFont) {
  const CSSProperty* properties[] = {
      &GetCSSPropertyTextOrientation(),
      &GetCSSPropertyWebkitTextOrientation(),
  };

  HeapVector<Member<const CSSValue>> values = {
      CSSInitialValue::Create(),
      CSSInheritedValue::Create(),
      CSSIdentifierValue::Create(CSSValueID::kMixed),
  };

  for (const CSSProperty* property : properties) {
    for (const CSSValue* value : values) {
      auto parent_style =
          GetDocument().GetStyleResolver().CreateComputedStyle();
      StyleResolverState state(GetDocument(), *GetDocument().body(),
                               nullptr /* StyleRecalcContext */,
                               StyleRequest(parent_style.get()));
      state.SetStyle(GetDocument().GetStyleResolver().CreateComputedStyle());

      // This test assumes that initial 'text-orientation' is not 'upright'.
      ASSERT_NE(ETextOrientation::kUpright,
                state.StyleBuilder().GetTextOrientation());
      state.StyleBuilder().SetTextOrientation(ETextOrientation::kUpright);

      ASSERT_FALSE(state.GetFontBuilder().FontDirty());
      StyleBuilder::ApplyProperty(*property, state,
                                  ScopedCSSValue(*value, &GetDocument()));
      EXPECT_TRUE(state.GetFontBuilder().FontDirty());
    }
  }
}

TEST_F(StyleBuilderTest, HasExplicitInheritance) {
  auto parent_style = GetDocument().GetStyleResolver().CreateComputedStyle();
  auto style = GetDocument().GetStyleResolver().CreateComputedStyle();
  StyleResolverState state(GetDocument(), *GetDocument().body(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(parent_style.get()));
  state.SetStyle(style);
  EXPECT_FALSE(style->HasExplicitInheritance());

  ScopedCSSValue inherited(*CSSInheritedValue::Create(), &GetDocument());

  // Flag should not be set for properties which are inherited.
  StyleBuilder::ApplyProperty(GetCSSPropertyColor(), state, inherited);
  EXPECT_FALSE(style->HasExplicitInheritance());

  StyleBuilder::ApplyProperty(GetCSSPropertyBackgroundColor(), state,
                              inherited);
  EXPECT_TRUE(style->HasExplicitInheritance());
}

TEST_F(StyleBuilderTest, GridTemplateAreasApplyOrder) {
  const CSSProperty& grid_template_areas = GetCSSPropertyGridTemplateAreas();
  const CSSProperty& grid_template_rows = GetCSSPropertyGridTemplateRows();
  const CSSProperty& grid_template_columns =
      GetCSSPropertyGridTemplateColumns();

  const CSSValue* grid_template_areas_value = css_test_helpers::ParseLonghand(
      GetDocument(), grid_template_areas, "'foo' 'bar' 'baz' 'faz'");
  const CSSValue* grid_template_columns_value = css_test_helpers::ParseLonghand(
      GetDocument(), grid_template_columns, "50px 50px");
  const CSSValue* grid_template_rows_value = css_test_helpers::ParseLonghand(
      GetDocument(), grid_template_rows, "50px 50px");

  ASSERT_TRUE(grid_template_areas_value);
  ASSERT_TRUE(grid_template_columns_value);
  ASSERT_TRUE(grid_template_rows_value);

  scoped_refptr<ComputedStyle> parent_style =
      GetDocument().GetStyleResolver().CreateComputedStyle();
  StyleResolverState state(GetDocument(), *GetDocument().body(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(parent_style.get()));

  scoped_refptr<ComputedStyle> style1;
  scoped_refptr<ComputedStyle> style2;

  // grid-template-areas applied first.
  state.SetStyle(ComputedStyle::Clone(*parent_style));
  StyleBuilder::ApplyProperty(
      grid_template_areas, state,
      ScopedCSSValue(*grid_template_areas_value, nullptr));
  StyleBuilder::ApplyProperty(
      grid_template_columns, state,
      ScopedCSSValue(*grid_template_columns_value, nullptr));
  StyleBuilder::ApplyProperty(
      grid_template_rows, state,
      ScopedCSSValue(*grid_template_rows_value, nullptr));
  style1 = state.TakeStyle();

  // grid-template-areas applied last.
  state.SetStyle(ComputedStyle::Clone(*parent_style));
  StyleBuilder::ApplyProperty(
      grid_template_columns, state,
      ScopedCSSValue(*grid_template_columns_value, nullptr));
  StyleBuilder::ApplyProperty(
      grid_template_rows, state,
      ScopedCSSValue(*grid_template_rows_value, nullptr));
  StyleBuilder::ApplyProperty(
      grid_template_areas, state,
      ScopedCSSValue(*grid_template_areas_value, nullptr));
  style2 = state.TakeStyle();

  ASSERT_TRUE(style1);
  ASSERT_TRUE(style2);
  EXPECT_EQ(*style1, *style2)
      << "Application order of grid properties does not affect result";
}

}  // namespace blink
