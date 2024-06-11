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
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

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
      const auto& parent_style =
          GetDocument().GetStyleResolver().InitialStyle();
      StyleResolverState state(GetDocument(), *GetDocument().body(),
                               nullptr /* StyleRecalcContext */,
                               StyleRequest(&parent_style));
      state.SetStyle(GetDocument().GetStyleResolver().InitialStyle());

      // This test assumes that initial 'writing-mode' is not 'vertical-lr'.
      ASSERT_NE(WritingMode::kVerticalLr,
                state.StyleBuilder().GetWritingMode());
      state.StyleBuilder().SetWritingMode(WritingMode::kVerticalLr);

      ASSERT_FALSE(state.GetFontBuilder().FontDirty());
      StyleBuilder::ApplyProperty(*property, state, *value);
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
      const auto& parent_style =
          GetDocument().GetStyleResolver().InitialStyle();
      StyleResolverState state(GetDocument(), *GetDocument().body(),
                               nullptr /* StyleRecalcContext */,
                               StyleRequest(&parent_style));
      state.SetStyle(GetDocument().GetStyleResolver().InitialStyle());

      // This test assumes that initial 'text-orientation' is not 'upright'.
      ASSERT_NE(ETextOrientation::kUpright,
                state.StyleBuilder().GetTextOrientation());
      state.StyleBuilder().SetTextOrientation(ETextOrientation::kUpright);

      ASSERT_FALSE(state.GetFontBuilder().FontDirty());
      StyleBuilder::ApplyProperty(*property, state, *value);
      EXPECT_TRUE(state.GetFontBuilder().FontDirty());
    }
  }
}

TEST_F(StyleBuilderTest, HasExplicitInheritance) {
  const auto& parent_style = GetDocument().GetStyleResolver().InitialStyle();
  StyleResolverState state(GetDocument(), *GetDocument().body(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(&parent_style));
  state.SetStyle(GetDocument().GetStyleResolver().InitialStyle());
  EXPECT_FALSE(state.StyleBuilder().HasExplicitInheritance());

  const CSSValue& inherited = *CSSInheritedValue::Create();

  // Flag should not be set for properties which are inherited.
  StyleBuilder::ApplyProperty(GetCSSPropertyColor(), state, inherited);
  EXPECT_FALSE(state.StyleBuilder().HasExplicitInheritance());

  StyleBuilder::ApplyProperty(GetCSSPropertyBackgroundColor(), state,
                              inherited);
  EXPECT_TRUE(state.StyleBuilder().HasExplicitInheritance());
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

  const ComputedStyle& parent_style =
      GetDocument().GetStyleResolver().InitialStyle();
  StyleResolverState state(GetDocument(), *GetDocument().body(),
                           nullptr /* StyleRecalcContext */,
                           StyleRequest(&parent_style));

  // grid-template-areas applied first.
  state.SetStyle(parent_style);
  StyleBuilder::ApplyProperty(grid_template_areas, state,
                              *grid_template_areas_value);
  StyleBuilder::ApplyProperty(grid_template_columns, state,
                              *grid_template_columns_value);
  StyleBuilder::ApplyProperty(grid_template_rows, state,
                              *grid_template_rows_value);
  const ComputedStyle* style1 = state.TakeStyle();

  // grid-template-areas applied last.
  state.SetStyle(parent_style);
  StyleBuilder::ApplyProperty(grid_template_columns, state,
                              *grid_template_columns_value);
  StyleBuilder::ApplyProperty(grid_template_rows, state,
                              *grid_template_rows_value);
  StyleBuilder::ApplyProperty(grid_template_areas, state,
                              *grid_template_areas_value);
  const ComputedStyle* style2 = state.TakeStyle();

  ASSERT_TRUE(style1);
  ASSERT_TRUE(style2);
  EXPECT_EQ(*style1, *style2)
      << "Application order of grid properties does not affect result";
}

}  // namespace blink
