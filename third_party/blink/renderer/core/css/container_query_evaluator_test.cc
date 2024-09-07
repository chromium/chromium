// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ContainerQueryEvaluatorTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    GetDocument().body()->setInnerHTML(R"HTML(
      <div id="container-parent">
        <div id="container"></div>
      </div>
    )HTML");
  }

  Element& ContainerElement() {
    return *GetDocument().getElementById(AtomicString("container"));
  }

  ContainerQuery* ParseContainer(String query) {
    String rule = "@container " + query + " {}";
    auto* style_rule = DynamicTo<StyleRuleContainer>(
        css_test_helpers::ParseRule(GetDocument(), rule));
    if (!style_rule) {
      return nullptr;
    }
    return &style_rule->GetContainerQuery();
  }

  ContainerQueryEvaluator* CreateEvaluatorForType(unsigned container_type) {
    ComputedStyleBuilder builder(
        *GetDocument().GetStyleResolver().InitialStyleForElement());
    builder.SetContainerType(container_type);
    ContainerElement().SetComputedStyle(builder.TakeStyle());
    return MakeGarbageCollected<ContainerQueryEvaluator>(ContainerElement());
  }

  bool Eval(String query,
            double width,
            double height,
            unsigned container_type,
            PhysicalAxes contained_axes) {
    ContainerQuery* container_query = ParseContainer(query);
    DCHECK(container_query);
    ContainerQueryEvaluator* evaluator = CreateEvaluatorForType(container_type);
    evaluator->SizeContainerChanged(
        PhysicalSize(LayoutUnit(width), LayoutUnit(height)), contained_axes);
    return evaluator->Eval(*container_query).value;
  }

  bool Eval(String query,
            String custom_property_name,
            String custom_property_value) {
    const CSSParserContext* context =
        StrictCSSParserContext(SecureContextMode::kSecureContext);
    CSSUnparsedDeclarationValue* value =
        CSSVariableParser::ParseDeclarationValue(custom_property_value, false,
                                                 *context);
    DCHECK(value);

    ComputedStyleBuilder builder =
        GetDocument().GetStyleResolver().InitialStyleBuilderForElement();
    builder.SetVariableData(AtomicString(custom_property_name),
                            value->VariableDataValue(), false);
    ContainerElement().SetComputedStyle(builder.TakeStyle());

    auto* evaluator =
        MakeGarbageCollected<ContainerQueryEvaluator>(ContainerElement());
    evaluator->SizeContainerChanged(
        PhysicalSize(LayoutUnit(100), LayoutUnit(100)),
        PhysicalAxes{kPhysicalAxesNone});

    ContainerQuery* container_query = ParseContainer(query);
    return evaluator->Eval(*container_query).value;
  }

  using Change = ContainerQueryEvaluator::Change;

  Change SizeContainerChanged(ContainerQueryEvaluator* evaluator,
                              PhysicalSize size,
                              unsigned container_type,
                              PhysicalAxes axes) {
    ComputedStyleBuilder builder(
        *GetDocument().GetStyleResolver().InitialStyleForElement());
    builder.SetContainerType(container_type);
    ContainerElement().SetComputedStyle(builder.TakeStyle());
    return evaluator->SizeContainerChanged(size, axes);
  }

  Change StickyContainerChanged(ContainerQueryEvaluator* evaluator,
                                ContainerStuckPhysical stuck_horizontal,
                                ContainerStuckPhysical stuck_vertical,
                                unsigned container_type) {
    ComputedStyleBuilder builder(
        *GetDocument().GetStyleResolver().InitialStyleForElement());
    builder.SetContainerType(container_type);
    ContainerElement().SetComputedStyle(builder.TakeStyle());
    return evaluator->StickyContainerChanged(stuck_horizontal, stuck_vertical);
  }

  Change SnapContainerChanged(ContainerQueryEvaluator* evaluator,
                              ContainerSnappedFlags snapped,
                              unsigned container_type) {
    ComputedStyleBuilder builder(
        *GetDocument().GetStyleResolver().InitialStyleForElement());
    builder.SetContainerType(container_type);
    ContainerElement().SetComputedStyle(builder.TakeStyle());
    return evaluator->SnapContainerChanged(snapped);
  }

  bool EvalAndAdd(ContainerQueryEvaluator* evaluator,
                  const ContainerQuery& query,
                  Change change = Change::kNearestContainer) {
    MatchResult dummy_result;
    return evaluator->EvalAndAdd(query, change, dummy_result);
  }

  using Result = ContainerQueryEvaluator::Result;
  const HeapHashMap<Member<const ContainerQuery>, Result>& GetResults(
      ContainerQueryEvaluator* evaluator) const {
    return evaluator->results_;
  }

  unsigned GetUnitFlags(ContainerQueryEvaluator* evaluator) const {
    return evaluator->unit_flags_;
  }

  void ClearSizeResults(ContainerQueryEvaluator* evaluator,
                        Change change) const {
    return evaluator->ClearResults(change,
                                   ContainerQueryEvaluator::kSizeContainer);
  }

  void ClearStyleResults(ContainerQueryEvaluator* evaluator,
                         Change change) const {
    return evaluator->ClearResults(change,
                                   ContainerQueryEvaluator::kStyleContainer);
  }

  const PhysicalAxes none{kPhysicalAxesNone};
  const PhysicalAxes both{kPhysicalAxesBoth};
  const PhysicalAxes horizontal{kPhysicalAxesHorizontal};
  const PhysicalAxes vertical{kPhysicalAxesVertical};

  const unsigned type_normal = kContainerTypeNormal;
  const unsigned type_size = kContainerTypeSize;
  const unsigned type_inline_size = kContainerTypeInlineSize;
  const unsigned type_scroll_state = kContainerTypeScrollState;
};

TEST_F(ContainerQueryEvaluatorTest, ContainmentMatch) {
  {
    String query = "(min-width: 100px)";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, horizontal));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, both));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_inline_size, horizontal));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_inline_size, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, vertical));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, none));
    EXPECT_FALSE(Eval(query, 99.0, 100.0, type_size, horizontal));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_normal, both));
  }

  {
    String query = "(min-height: 100px)";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, vertical));
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, horizontal));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, none));
    EXPECT_FALSE(Eval(query, 100.0, 99.0, type_size, vertical));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_normal, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_inline_size, both));
  }

  {
    String query = "((min-width: 100px) and (min-height: 100px))";
    EXPECT_TRUE(Eval(query, 100.0, 100.0, type_size, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, vertical));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, horizontal));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_size, none));
    EXPECT_FALSE(Eval(query, 100.0, 99.0, type_size, both));
    EXPECT_FALSE(Eval(query, 99.0, 100.0, type_size, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_normal, both));
    EXPECT_FALSE(Eval(query, 100.0, 100.0, type_inline_size, both));
  }
}

TEST_F(ContainerQueryEvaluatorTest, SizeContainerChanged) {
  PhysicalSize size_50(LayoutUnit(50), LayoutUnit(50));
  PhysicalSize size_100(LayoutUnit(100), LayoutUnit(100));
  PhysicalSize size_200(LayoutUnit(200), LayoutUnit(200));

  ContainerQuery* container_query_50 = ParseContainer("(min-width: 50px)");
  ContainerQuery* container_query_100 = ParseContainer("(min-width: 100px)");
  ContainerQuery* container_query_200 = ParseContainer("(min-width: 200px)");
  ASSERT_TRUE(container_query_50);
  ASSERT_TRUE(container_query_100);
  ASSERT_TRUE(container_query_200);

  ContainerQueryEvaluator* evaluator = CreateEvaluatorForType(type_inline_size);
  SizeContainerChanged(evaluator, size_100, type_size, horizontal);

  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_100));
  EXPECT_FALSE(EvalAndAdd(evaluator, *container_query_200));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Calling SizeContainerChanged with the values we already have should not
  // produce a Change.
  EXPECT_EQ(Change::kNone,
            SizeContainerChanged(evaluator, size_100, type_size, horizontal));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // EvalAndAdding the same queries again is allowed.
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_100));
  EXPECT_FALSE(EvalAndAdd(evaluator, *container_query_200));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Resize from 100px to 200px.
  EXPECT_EQ(Change::kNearestContainer,
            SizeContainerChanged(evaluator, size_200, type_size, horizontal));
  EXPECT_EQ(0u, GetResults(evaluator).size());

  // Now both 100px and 200px queries should return true.
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_100));
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_200));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Calling SizeContainerChanged with the values we already have should not
  // produce a Change.
  EXPECT_EQ(Change::kNone,
            SizeContainerChanged(evaluator, size_200, type_size, horizontal));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Still valid to EvalAndAdd the same queries again.
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_100));
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_200));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Setting contained_axes=vertical should invalidate the queries, since
  // they query width.
  EXPECT_EQ(Change::kNearestContainer,
            SizeContainerChanged(evaluator, size_200, type_size, vertical));
  EXPECT_EQ(0u, GetResults(evaluator).size());

  EXPECT_FALSE(EvalAndAdd(evaluator, *container_query_100));
  EXPECT_FALSE(EvalAndAdd(evaluator, *container_query_200));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Switching back to horizontal.
  EXPECT_EQ(Change::kNearestContainer,
            SizeContainerChanged(evaluator, size_100, type_size, horizontal));
  EXPECT_EQ(0u, GetResults(evaluator).size());

  // Resize to 200px.
  EXPECT_EQ(Change::kNone,
            SizeContainerChanged(evaluator, size_200, type_size, horizontal));
  EXPECT_EQ(0u, GetResults(evaluator).size());

  // Add a query of each Change type.
  EXPECT_TRUE(
      EvalAndAdd(evaluator, *container_query_100, Change::kNearestContainer));
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_200,
                         Change::kDescendantContainers));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Resize to 50px should cause both queries to change their evaluation.
  // `ContainerChanged` should return the biggest `Change`.
  EXPECT_EQ(Change::kDescendantContainers,
            SizeContainerChanged(evaluator, size_50, type_size, horizontal));
}

TEST_F(ContainerQueryEvaluatorTest, StyleContainerChanged) {
  PhysicalSize size_100(LayoutUnit(100), LayoutUnit(100));

  Element& container_element = ContainerElement();
  ComputedStyleBuilder builder(
      *GetDocument().GetStyleResolver().InitialStyleForElement());
  builder.SetContainerType(type_inline_size);
  const ComputedStyle* style = builder.TakeStyle();
  container_element.SetComputedStyle(style);

  ContainerQueryEvaluator* evaluator = CreateEvaluatorForType(type_inline_size);
  EXPECT_EQ(Change::kNone,
            evaluator->SizeContainerChanged(size_100, horizontal));

  ContainerQuery* foo_bar_query = ParseContainer("style(--foo: bar)");
  ContainerQuery* size_bar_foo_query =
      ParseContainer("(inline-size = 100px) and style(--bar: foo)");
  ContainerQuery* no_match_query =
      ParseContainer("(inline-size > 1000px) and style(--no: match)");
  ASSERT_TRUE(foo_bar_query);
  ASSERT_TRUE(size_bar_foo_query);
  ASSERT_TRUE(no_match_query);

  auto eval_and_add_all = [&]() {
    EvalAndAdd(evaluator, *foo_bar_query);
    EvalAndAdd(evaluator, *size_bar_foo_query);
    EvalAndAdd(evaluator, *no_match_query);
  };

  eval_and_add_all();

  // Calling StyleContainerChanged without changing the style should not produce
  // a change.
  EXPECT_EQ(Change::kNone, evaluator->StyleContainerChanged());
  EXPECT_EQ(3u, GetResults(evaluator).size());

  const bool inherited = true;

  // Set --no: match. Should not cause change because size query part does not
  // match.
  builder = ComputedStyleBuilder(*style);
  builder.SetVariableData(AtomicString("--no"),
                          css_test_helpers::CreateVariableData("match"),
                          inherited);
  style = builder.TakeStyle();
  container_element.SetComputedStyle(style);
  EXPECT_EQ(Change::kNone, evaluator->StyleContainerChanged());
  EXPECT_EQ(3u, GetResults(evaluator).size());

  // Set --foo: bar. Should trigger change.
  builder = ComputedStyleBuilder(*style);
  builder.SetVariableData(AtomicString("--foo"),
                          css_test_helpers::CreateVariableData("bar"),
                          inherited);
  style = builder.TakeStyle();
  container_element.SetComputedStyle(style);
  EXPECT_EQ(Change::kNearestContainer, evaluator->StyleContainerChanged());
  EXPECT_EQ(0u, GetResults(evaluator).size());

  // Set --bar: foo. Should trigger change because size part also matches.
  eval_and_add_all();
  builder = ComputedStyleBuilder(*style);
  builder.SetVariableData(AtomicString("--bar"),
                          css_test_helpers::CreateVariableData("foo"),
                          inherited);
  style = builder.TakeStyle();
  container_element.SetComputedStyle(style);
  EXPECT_EQ(Change::kNearestContainer, evaluator->StyleContainerChanged());
  EXPECT_EQ(0u, GetResults(evaluator).size());
}

TEST_F(ContainerQueryEvaluatorTest, StickyContainerChanged) {
  ContainerQuery* container_query_left =
      ParseContainer("scroll-state(stuck: left)");
  ContainerQuery* container_query_bottom =
      ParseContainer("scroll-state(stuck: bottom)");
  ASSERT_TRUE(container_query_left);
  ASSERT_TRUE(container_query_bottom);

  ContainerQueryEvaluator* evaluator =
      CreateEvaluatorForType(type_scroll_state);
  StickyContainerChanged(evaluator, ContainerStuckPhysical::kLeft,
                         ContainerStuckPhysical::kNo, type_scroll_state);

  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_left));
  EXPECT_FALSE(EvalAndAdd(evaluator, *container_query_bottom));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Calling StickyContainerChanged with the values we already have should not
  // produce a Change.
  EXPECT_EQ(Change::kNone, StickyContainerChanged(
                               evaluator, ContainerStuckPhysical::kLeft,
                               ContainerStuckPhysical::kNo, type_scroll_state));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // EvalAndAdding the same queries again is allowed.
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_left));
  EXPECT_FALSE(EvalAndAdd(evaluator, *container_query_bottom));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Set vertically stuck to bottom.
  EXPECT_EQ(Change::kNearestContainer,
            StickyContainerChanged(evaluator, ContainerStuckPhysical::kLeft,
                                   ContainerStuckPhysical::kBottom,
                                   type_scroll_state));
  EXPECT_EQ(0u, GetResults(evaluator).size());

  // Now both left and bottom queries should return true.
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_left));
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_bottom));
  EXPECT_EQ(2u, GetResults(evaluator).size());
}

TEST_F(ContainerQueryEvaluatorTest, SnapContainerChanged) {
  ContainerQuery* container_query_snap_block =
      ParseContainer("scroll-state(snapped: block)");
  ContainerQuery* container_query_snap_inline =
      ParseContainer("scroll-state(snapped: inline)");
  ASSERT_TRUE(container_query_snap_block);
  ASSERT_TRUE(container_query_snap_inline);

  ContainerQueryEvaluator* evaluator =
      CreateEvaluatorForType(type_scroll_state);
  SnapContainerChanged(evaluator,
                       static_cast<ContainerSnappedFlags>(ContainerSnapped::kY),
                       type_scroll_state);

  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_snap_block));
  EXPECT_FALSE(EvalAndAdd(evaluator, *container_query_snap_inline));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Calling SnapContainerChanged with the values we already have should not
  // produce a Change.
  EXPECT_EQ(
      Change::kNone,
      SnapContainerChanged(
          evaluator, static_cast<ContainerSnappedFlags>(ContainerSnapped::kY),
          type_scroll_state));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // EvalAndAdding the same queries again is allowed.
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_snap_block));
  EXPECT_FALSE(EvalAndAdd(evaluator, *container_query_snap_inline));
  EXPECT_EQ(2u, GetResults(evaluator).size());

  // Add inline snapped.
  EXPECT_EQ(Change::kNearestContainer,
            SnapContainerChanged(
                evaluator,
                static_cast<ContainerSnappedFlags>(ContainerSnapped::kX) |
                    static_cast<ContainerSnappedFlags>(ContainerSnapped::kY),
                type_scroll_state));
  EXPECT_EQ(0u, GetResults(evaluator).size());

  // Now both block and inline queries should return true.
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_snap_block));
  EXPECT_TRUE(EvalAndAdd(evaluator, *container_query_snap_inline));
  EXPECT_EQ(2u, GetResults(evaluator).size());
}

TEST_F(ContainerQueryEvaluatorTest, ClearResults) {
  PhysicalSize size_100(LayoutUnit(100), LayoutUnit(100));

  ContainerQuery* container_query_px = ParseContainer("(min-width: 50px)");
  ContainerQuery* container_query_em = ParseContainer("(min-width: 10em)");
  ContainerQuery* container_query_vh = ParseContainer("(min-width: 10vh)");
  ContainerQuery* container_query_cqw = ParseContainer("(min-width: 10cqw)");
  ContainerQuery* container_query_style = ParseContainer("style(--foo: bar)");
  ContainerQuery* container_query_size_and_style =
      ParseContainer("(width > 0px) and style(--foo: bar)");
  ASSERT_TRUE(container_query_px);
  ASSERT_TRUE(container_query_em);
  ASSERT_TRUE(container_query_vh);
  ASSERT_TRUE(container_query_cqw);
  ASSERT_TRUE(container_query_style);
  ASSERT_TRUE(container_query_size_and_style);

  ContainerQueryEvaluator* evaluator = CreateEvaluatorForType(type_inline_size);
  SizeContainerChanged(evaluator, size_100, type_size, horizontal);

  EXPECT_EQ(0u, GetResults(evaluator).size());

  using UnitFlags = MediaQueryExpValue::UnitFlags;

  // EvalAndAdd (min-width: 50px), nearest.
  EvalAndAdd(evaluator, *container_query_px, Change::kNearestContainer);
  ASSERT_EQ(1u, GetResults(evaluator).size());
  EXPECT_EQ(Change::kNearestContainer,
            GetResults(evaluator).at(container_query_px).change);
  EXPECT_EQ(UnitFlags::kNone,
            GetResults(evaluator).at(container_query_px).unit_flags);
  EXPECT_EQ(UnitFlags::kNone, GetUnitFlags(evaluator));

  // EvalAndAdd (min-width: 10em), descendant
  EvalAndAdd(evaluator, *container_query_em, Change::kDescendantContainers);
  ASSERT_EQ(2u, GetResults(evaluator).size());
  EXPECT_EQ(Change::kDescendantContainers,
            GetResults(evaluator).at(container_query_em).change);
  EXPECT_EQ(UnitFlags::kFontRelative,
            GetResults(evaluator).at(container_query_em).unit_flags);
  EXPECT_EQ(UnitFlags::kFontRelative, GetUnitFlags(evaluator));

  // EvalAndAdd (min-width: 10vh), nearest
  EvalAndAdd(evaluator, *container_query_vh, Change::kNearestContainer);
  ASSERT_EQ(3u, GetResults(evaluator).size());
  EXPECT_EQ(Change::kNearestContainer,
            GetResults(evaluator).at(container_query_vh).change);
  EXPECT_EQ(UnitFlags::kStaticViewport,
            GetResults(evaluator).at(container_query_vh).unit_flags);
  EXPECT_EQ(static_cast<unsigned>(UnitFlags::kFontRelative |
                                  UnitFlags::kStaticViewport),
            GetUnitFlags(evaluator));

  // EvalAndAdd (min-width: 10cqw), descendant
  EvalAndAdd(evaluator, *container_query_cqw, Change::kDescendantContainers);
  ASSERT_EQ(4u, GetResults(evaluator).size());
  EXPECT_EQ(Change::kDescendantContainers,
            GetResults(evaluator).at(container_query_cqw).change);
  EXPECT_EQ(UnitFlags::kContainer,
            GetResults(evaluator).at(container_query_cqw).unit_flags);
  EXPECT_EQ(
      static_cast<unsigned>(UnitFlags::kFontRelative |
                            UnitFlags::kStaticViewport | UnitFlags::kContainer),
      GetUnitFlags(evaluator));

  // Make sure clearing style() results does not clear any size results.
  ClearStyleResults(evaluator, Change::kDescendantContainers);
  ASSERT_EQ(4u, GetResults(evaluator).size());

  // Clearing kNearestContainer should leave all information originating
  // from kDescendantContainers.
  ClearSizeResults(evaluator, Change::kNearestContainer);
  ASSERT_EQ(2u, GetResults(evaluator).size());
  EXPECT_EQ(Change::kDescendantContainers,
            GetResults(evaluator).at(container_query_em).change);
  EXPECT_EQ(Change::kDescendantContainers,
            GetResults(evaluator).at(container_query_cqw).change);
  EXPECT_EQ(UnitFlags::kFontRelative,
            GetResults(evaluator).at(container_query_em).unit_flags);
  EXPECT_EQ(UnitFlags::kContainer,
            GetResults(evaluator).at(container_query_cqw).unit_flags);
  EXPECT_EQ(
      static_cast<unsigned>(UnitFlags::kFontRelative | UnitFlags::kContainer),
      GetUnitFlags(evaluator));

  // Clearing Change::kDescendantContainers should clear everything.
  ClearSizeResults(evaluator, Change::kDescendantContainers);
  ASSERT_EQ(0u, GetResults(evaluator).size());
  EXPECT_EQ(UnitFlags::kNone, GetUnitFlags(evaluator));

  // Add everything again, to ensure that
  // ClearResults(Change::kDescendantContainers, ...) also clears
  // Change::kNearestContainer.
  EvalAndAdd(evaluator, *container_query_px, Change::kNearestContainer);
  EvalAndAdd(evaluator, *container_query_em, Change::kDescendantContainers);
  EvalAndAdd(evaluator, *container_query_vh, Change::kNearestContainer);
  EvalAndAdd(evaluator, *container_query_cqw, Change::kDescendantContainers);
  ASSERT_EQ(4u, GetResults(evaluator).size());
  EXPECT_EQ(
      static_cast<unsigned>(UnitFlags::kFontRelative |
                            UnitFlags::kStaticViewport | UnitFlags::kContainer),
      GetUnitFlags(evaluator));
  ClearSizeResults(evaluator, Change::kDescendantContainers);
  ASSERT_EQ(0u, GetResults(evaluator).size());
  EXPECT_EQ(UnitFlags::kNone, GetUnitFlags(evaluator));

  // Clearing style() results
  EvalAndAdd(evaluator, *container_query_px, Change::kNearestContainer);
  EvalAndAdd(evaluator, *container_query_style, Change::kDescendantContainers);
  EvalAndAdd(evaluator, *container_query_size_and_style,
             Change::kNearestContainer);

  EXPECT_EQ(3u, GetResults(evaluator).size());
  ClearStyleResults(evaluator, Change::kNearestContainer);
  EXPECT_EQ(2u, GetResults(evaluator).size());

  EvalAndAdd(evaluator, *container_query_px, Change::kNearestContainer);
  EvalAndAdd(evaluator, *container_query_style, Change::kDescendantContainers);
  EvalAndAdd(evaluator, *container_query_size_and_style,
             Change::kNearestContainer);

  EXPECT_EQ(3u, GetResults(evaluator).size());
  ClearStyleResults(evaluator, Change::kDescendantContainers);
  EXPECT_EQ(1u, GetResults(evaluator).size());

  ClearSizeResults(evaluator, Change::kNearestContainer);
  EXPECT_EQ(0u, GetResults(evaluator).size());
}

TEST_F(ContainerQueryEvaluatorTest, SizeInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #container {
        container-type: size;
        width: 500px;
        height: 500px;
      }
      @container (min-width: 500px) {
        div { z-index:1; }
      }
    </style>
    <div id=container>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
      <div id=div></div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);
  ASSERT_TRUE(container->GetContainerQueryEvaluator());

  {
    // Causes re-layout, but the size does not change
    container->SetInlineStyleProperty(CSSPropertyID::kFloat, "left");

    unsigned before_count = GetStyleEngine().StyleForElementCount();

    UpdateAllLifecyclePhasesForTest();

    unsigned after_count = GetStyleEngine().StyleForElementCount();

    // Only #container should be affected. In particular, we should not
    // recalc any style for <div> children of #container.
    EXPECT_EQ(1u, after_count - before_count);
  }

  {
    // The size of the container changes, but it does not matter for
    // the result of the query (min-width: 500px).
    container->SetInlineStyleProperty(CSSPropertyID::kWidth, "600px");

    unsigned before_count = GetStyleEngine().StyleForElementCount();

    UpdateAllLifecyclePhasesForTest();

    unsigned after_count = GetStyleEngine().StyleForElementCount();

    // Only #container should be affected. In particular, we should not
    // recalc any style for <div> children of #container.
    EXPECT_EQ(1u, after_count - before_count);
  }
}

TEST_F(ContainerQueryEvaluatorTest, DependentQueries) {
  PhysicalSize size_100(LayoutUnit(100), LayoutUnit(100));
  PhysicalSize size_150(LayoutUnit(150), LayoutUnit(150));
  PhysicalSize size_200(LayoutUnit(200), LayoutUnit(200));
  PhysicalSize size_300(LayoutUnit(300), LayoutUnit(300));
  PhysicalSize size_400(LayoutUnit(400), LayoutUnit(400));

  ContainerQuery* query_min_200px = ParseContainer("(min-width: 200px)");
  ContainerQuery* query_max_300px = ParseContainer("(max-width: 300px)");
  ASSERT_TRUE(query_min_200px);

  ContainerQueryEvaluator* evaluator = CreateEvaluatorForType(type_inline_size);
  SizeContainerChanged(evaluator, size_100, type_size, horizontal);

  EvalAndAdd(evaluator, *query_min_200px);
  EvalAndAdd(evaluator, *query_max_300px);
  // Updating with the same size as we initially had should not invalidate
  // any query results.
  EXPECT_EQ(Change::kNone,
            SizeContainerChanged(evaluator, size_100, type_size, horizontal));

  // Makes no difference for either of (min-width: 200px), (max-width: 300px):
  EXPECT_EQ(Change::kNone,
            SizeContainerChanged(evaluator, size_150, type_size, horizontal));

  // (min-width: 200px) becomes true:
  EXPECT_EQ(Change::kNearestContainer,
            SizeContainerChanged(evaluator, size_200, type_size, horizontal));

  EvalAndAdd(evaluator, *query_min_200px);
  EvalAndAdd(evaluator, *query_max_300px);
  EXPECT_EQ(Change::kNone,
            SizeContainerChanged(evaluator, size_200, type_size, horizontal));

  // Makes no difference for either of (min-width: 200px), (max-width: 300px):
  EXPECT_EQ(Change::kNone,
            SizeContainerChanged(evaluator, size_300, type_size, horizontal));

  // (max-width: 300px) becomes false:
  EXPECT_EQ(Change::kNearestContainer,
            SizeContainerChanged(evaluator, size_400, type_size, horizontal));
}

TEST_F(ContainerQueryEvaluatorTest, EvaluatorDisplayNone) {
  SetBodyInnerHTML(R"HTML(
    <style>
      main {
        display: block;
        container-type: size;
        width: 500px;
        height: 500px;
      }
      main.none {
        display: none;
      }
      @container (min-width: 500px) {
        div { --x:test; }
      }
    </style>
    <main id=outer>
      <div>
        <main id=inner>
          <div></div>
        </main>
      </div>
    </main>
  )HTML");

  // Inner container
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  ASSERT_TRUE(inner);
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());

  inner->classList().Add(AtomicString("none"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(inner->GetContainerQueryEvaluator());

  inner->classList().Remove(AtomicString("none"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(inner->GetContainerQueryEvaluator());

  // Outer container
  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  ASSERT_TRUE(outer);
  EXPECT_TRUE(outer->GetContainerQueryEvaluator());
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());

  outer->classList().Add(AtomicString("none"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(outer->GetContainerQueryEvaluator());
  EXPECT_FALSE(inner->GetContainerQueryEvaluator());

  outer->classList().Remove(AtomicString("none"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(outer->GetContainerQueryEvaluator());
  EXPECT_TRUE(inner->GetContainerQueryEvaluator());
}

TEST_F(ContainerQueryEvaluatorTest, Printing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @page { size: 400px 400px; }
      body { margin: 0; }
      #container {
        container-type: size;
        width: 50vw;
      }

      @container (width = 200px) {
        #target { color: green; }
      }
    </style>
    <div id="container">
      <span id="target"></span>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_NE(
      target->ComputedStyleRef().VisitedDependentColor(GetCSSPropertyColor()),
      Color(0, 128, 0));

  constexpr gfx::SizeF initial_page_size(400, 400);
  GetDocument().GetFrame()->StartPrinting(WebPrintParams(initial_page_size));
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();

  EXPECT_EQ(
      target->ComputedStyleRef().VisitedDependentColor(GetCSSPropertyColor()),
      Color(0, 128, 0));
}

TEST_F(ContainerQueryEvaluatorTest, CustomPropertyStyleQuery) {
  EXPECT_FALSE(Eval("style(--my-prop)", "--my-prop", "10px"));
  EXPECT_FALSE(Eval("style(--my-prop:)", "--my-prop", "10px"));
  EXPECT_FALSE(Eval("style(--my-prop: )", "--my-prop", "10px"));

  EXPECT_FALSE(Eval("style(--my-prop)", "--my-prop", ""));
  EXPECT_TRUE(Eval("style(--my-prop:)", "--my-prop", ""));
  EXPECT_TRUE(Eval("style(--my-prop: )", "--my-prop", ""));

  EXPECT_TRUE(Eval("style(--my-prop:10px)", "--my-prop", "10px"));
  EXPECT_TRUE(Eval("style(--my-prop: 10px)", "--my-prop", "10px"));
  EXPECT_TRUE(Eval("style(--my-prop:10px )", "--my-prop", "10px"));
  EXPECT_TRUE(Eval("style(--my-prop: 10px )", "--my-prop", "10px"));
}

TEST_F(ContainerQueryEvaluatorTest, FindContainer) {
  SetBodyInnerHTML(R"HTML(
    <div style="container-name:outer;container-type:size">
      <div style="container-name:outer">
        <div style="container-type: size">
          <div>
            <div></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* outer_size = GetDocument().body()->firstElementChild();
  Element* outer = outer_size->firstElementChild();
  Element* inner_size = outer->firstElementChild();
  Element* inner = inner_size->firstElementChild();

  EXPECT_EQ(ContainerQueryEvaluator::FindContainer(
                inner, ParseContainer("style(--foo: bar)")->Selector(),
                &GetDocument()),
            inner);
  EXPECT_EQ(
      ContainerQueryEvaluator::FindContainer(
          inner,
          ParseContainer("(width > 100px) and style(--foo: bar)")->Selector(),
          &GetDocument()),
      inner_size);
  EXPECT_EQ(ContainerQueryEvaluator::FindContainer(
                inner, ParseContainer("outer style(--foo: bar)")->Selector(),
                &GetDocument()),
            outer);
  EXPECT_EQ(ContainerQueryEvaluator::FindContainer(
                inner,
                ParseContainer("outer (width > 100px) and style(--foo: bar)")
                    ->Selector(),
                &GetDocument()),
            outer_size);
}

TEST_F(ContainerQueryEvaluatorTest, FindStickyContainer) {
  SetBodyInnerHTML(R"HTML(
    <div style="container-type: scroll-state size">
      <div style="container-name:outer;container-type: scroll-state">
        <div style="container-name:outer">
          <div style="container-type: scroll-state">
            <div>
              <div></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* sticky_size = GetDocument().body()->firstElementChild();
  Element* outer_sticky = sticky_size->firstElementChild();
  Element* outer = outer_sticky->firstElementChild();
  Element* inner_sticky = outer->firstElementChild();
  Element* inner = inner_sticky->firstElementChild();

  EXPECT_EQ(ContainerQueryEvaluator::FindContainer(
                inner,
                ParseContainer("scroll-state(stuck: top) and style(--foo: bar)")
                    ->Selector(),
                &GetDocument()),
            inner_sticky);
  EXPECT_EQ(
      ContainerQueryEvaluator::FindContainer(
          inner,
          ParseContainer("outer scroll-state(stuck: top) and style(--foo: bar)")
              ->Selector(),
          &GetDocument()),
      outer_sticky);
  EXPECT_EQ(ContainerQueryEvaluator::FindContainer(
                inner,
                ParseContainer("scroll-state(stuck: top) and (width > 0px)")
                    ->Selector(),
                &GetDocument()),
            sticky_size);
}

TEST_F(ContainerQueryEvaluatorTest, FindSnapContainer) {
  SetBodyInnerHTML(R"HTML(
    <div style="container-type: scroll-state inline-size">
      <div style="container-name:outer;container-type: scroll-state">
        <div style="container-name:outer">
          <div style="container-type: scroll-state">
            <div>
              <div></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* sticky_snap = GetDocument().body()->firstElementChild();
  Element* outer_snap = sticky_snap->firstElementChild();
  Element* outer = outer_snap->firstElementChild();
  Element* inner_snap = outer->firstElementChild();
  Element* inner = inner_snap->firstElementChild();

  EXPECT_EQ(
      ContainerQueryEvaluator::FindContainer(
          inner,
          ParseContainer("scroll-state(snapped: inline) and style(--foo: bar)")
              ->Selector(),
          &GetDocument()),
      inner_snap);
  EXPECT_EQ(ContainerQueryEvaluator::FindContainer(
                inner,
                ParseContainer(
                    "outer scroll-state(snapped: block) and style(--foo: bar)")
                    ->Selector(),
                &GetDocument()),
            outer_snap);
  EXPECT_EQ(ContainerQueryEvaluator::FindContainer(
                inner,
                ParseContainer("scroll-state((snapped: none) and (stuck: "
                               "bottom)) and (width > 0px)")
                    ->Selector(),
                &GetDocument()),
            sticky_snap);
}

TEST_F(ContainerQueryEvaluatorTest, ScopedCaching) {
  GetDocument().documentElement()->setHTMLUnsafe(R"HTML(
    <div id="host" style="container-name: n1">
      <template shadowrootmode=open>
        <div style="container-name: n1">
          <slot id="slot"></slot>
        </div>
      </template>
      <div id="slotted"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  ContainerSelectorCache cache;
  StyleRecalcContext context;
  MatchResult result;
  ContainerQuery* query1 = ParseContainer("n1 style(--foo: bar)");
  ContainerQuery* query2 = ParseContainer("n1 style(--foo: bar)");

  ASSERT_TRUE(query1);
  ASSERT_TRUE(query2);

  //  Element* slotted = GetElementById("slotted");
  Element* host = GetElementById("host");
  ShadowRoot* shadow_root = host->GetShadowRoot();
  Element* slot = shadow_root->getElementById(AtomicString("slot"));

  result.BeginAddingAuthorRulesForTreeScope(*shadow_root);

  ContainerQueryEvaluator::EvalAndAdd(slot, context, *query1, cache, result);
  EXPECT_EQ(cache.size(), 1u);
  ContainerQueryEvaluator::EvalAndAdd(slot, context, *query1, cache, result);
  EXPECT_EQ(cache.size(), 1u);
  ContainerQueryEvaluator::EvalAndAdd(slot, context, *query2, cache, result);
  EXPECT_EQ(cache.size(), 1u);
  ContainerQueryEvaluator::EvalAndAdd(slot, context, *query2, cache, result);
  EXPECT_EQ(cache.size(), 1u);

  result.BeginAddingAuthorRulesForTreeScope(GetDocument());

  ContainerQueryEvaluator::EvalAndAdd(host, context, *query1, cache, result);
  EXPECT_EQ(cache.size(), 2u);
  ContainerQueryEvaluator::EvalAndAdd(host, context, *query2, cache, result);
  EXPECT_EQ(cache.size(), 2u);
}

TEST_F(ContainerQueryEvaluatorTest, DisplayContentsStyleQueryInvalidation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      /* Register --foo to avoid recalc due to inheritance. */
      @property --foo {
        syntax: "none|bar";
        inherits: false;
        initial-value: none;
      }
      #container.contents {
        --foo: bar;
        display: contents;
      }
      @container style(--foo: bar) {
        #container > div.bar {
          --match: true;
        }
      }
    </style>
    <div id="container">
      <div></div>
      <div></div>
      <div></div>
      <div class="bar"></div>
      <div></div>
      <div></div>
    </div>
  )HTML");

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);
  ContainerQueryEvaluator* evaluator = container->GetContainerQueryEvaluator();
  ASSERT_TRUE(evaluator);

  container->setAttribute(html_names::kClassAttr, AtomicString("contents"));

  unsigned before_count = GetStyleEngine().StyleForElementCount();

  UpdateAllLifecyclePhasesForTest();

  unsigned after_count = GetStyleEngine().StyleForElementCount();

  // #container and div.bar should be affected. In particular, we should not
  // recalc style for other <div> children of #container.
  EXPECT_EQ(2u, after_count - before_count);

  // The ContainerQueryEvaluator should still be the same. No need to re-create
  // the evaluator if when the display changes.
  EXPECT_EQ(evaluator, container->GetContainerQueryEvaluator());
}

struct EvalUnknownQueries {
  const char* query_string;
  bool contains_unknown;
};

EvalUnknownQueries eval_unknown_queries[] = {
    {"style(--foo: bar)", false},
    {"style(--foo: bar) or (foo: bar)", true},
    {"style(--foo: bar) and unknown()", true},
    {"style(font-size: 10px)", true},
    {"(width > 30px) and (height < 900px)", false},
    {"(width > 0px) or (unknown())", true},
    {"(height > 0px) and ((width > 20px) and unknown())", true},
    {"(not (unknown: 10px)) or (height)", true},
    {"(width: 'wide')", true},
};

class UseCountEvalUnknownTest
    : public ContainerQueryEvaluatorTest,
      public ::testing::WithParamInterface<EvalUnknownQueries> {};

INSTANTIATE_TEST_SUITE_P(ContainerQueryEvaluatorTest,
                         UseCountEvalUnknownTest,
                         testing::ValuesIn(eval_unknown_queries));

TEST_P(UseCountEvalUnknownTest, All) {
  EvalUnknownQueries param = GetParam();
  SCOPED_TRACE(param.query_string);

  Eval(param.query_string, 100.0, 100.0, type_size, horizontal);
  EXPECT_EQ(GetDocument().IsUseCounted(WebFeature::kContainerQueryEvalUnknown),
            param.contains_unknown);
}

}  // namespace blink
