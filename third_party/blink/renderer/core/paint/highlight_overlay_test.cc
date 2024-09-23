// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_overlay.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

namespace {

using HighlightLayerType = HighlightOverlay::HighlightLayerType;
using HighlightEdgeType = HighlightOverlay::HighlightEdgeType;
using HighlightLayer = HighlightOverlay::HighlightLayer;
using HighlightEdge = HighlightOverlay::HighlightEdge;
using HighlightPart = HighlightOverlay::HighlightPart;

}  // namespace

class HighlightOverlayTest : public PageTestBase {
 public:
  TextPaintStyle CreatePaintStyle(Color color) {
    return TextPaintStyle{color,   color,
                          color,   color,
                          2,       ::blink::mojom::blink::ColorScheme::kLight,
                          nullptr, TextDecorationLine::kNone,
                          color,   kPaintOrderNormal};
  }
};

TEST_F(HighlightOverlayTest, ComputeLayers) {
  SetBodyInnerHTML(R"HTML(foo)HTML");
  auto* text = DynamicTo<Text>(GetDocument().body()->firstChild());
  UpdateAllLifecyclePhasesForTest();

  LayoutSelectionStatus selection{0, 0, SelectSoftLineBreak::kNotSelected};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();
  const ComputedStyle& style = text->GetLayoutObject()->StyleRef();
  TextPaintStyle text_style;
  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);

  EXPECT_EQ(HighlightOverlay::ComputeLayers(GetDocument(), text, style,
                                            text_style, paint_info, nullptr,
                                            *none, *none, *none, *none, *none),
            HeapVector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating}})
      << "should return kOriginating when nothing is highlighted";

  EXPECT_EQ(HighlightOverlay::ComputeLayers(GetDocument(), text, style,
                                            text_style, paint_info, &selection,
                                            *none, *none, *none, *none, *none),
            (HeapVector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating},
                HighlightLayer{HighlightLayerType::kSelection},
            }))
      << "should return kSelection when a selection is given";

  auto* grammar = MakeGarbageCollected<DocumentMarkerVector>();
  auto* spelling = MakeGarbageCollected<DocumentMarkerVector>();
  auto* target = MakeGarbageCollected<DocumentMarkerVector>();
  auto* search = MakeGarbageCollected<DocumentMarkerVector>();
  grammar->push_back(MakeGarbageCollected<GrammarMarker>(1, 2, ""));
  grammar->push_back(MakeGarbageCollected<GrammarMarker>(2, 3, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(1, 2, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(2, 3, ""));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(1, 2));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(2, 3));
  search->push_back(MakeGarbageCollected<TextMatchMarker>(
      1, 2, TextMatchMarker::MatchStatus::kActive));
  search->push_back(MakeGarbageCollected<TextMatchMarker>(
      2, 3, TextMatchMarker::MatchStatus::kInactive));

  EXPECT_EQ(HighlightOverlay::ComputeLayers(
                GetDocument(), text, style, text_style, paint_info, nullptr,
                *none, *grammar, *spelling, *target, *search),
            (HeapVector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating},
                HighlightLayer{HighlightLayerType::kGrammar},
                HighlightLayer{HighlightLayerType::kSpelling},
                HighlightLayer{HighlightLayerType::kTargetText},
                HighlightLayer{HighlightLayerType::kSearchText},
                HighlightLayer{HighlightLayerType::kSearchTextCurrent},
            }))
      << "should return kGrammar + kSpelling + kTargetText + kSearchText no "
         "more than once each";

  HighlightRegistry* registry =
      HighlightRegistry::From(*text->GetDocument().domWindow());
  Range* highlight_range_1 = Range::Create(GetDocument());
  highlight_range_1->setStart(text, 0);
  highlight_range_1->setEnd(text, 1);
  Range* highlight_range_2 = Range::Create(GetDocument());
  highlight_range_2->setStart(text, 2);
  highlight_range_2->setEnd(text, 3);
  HeapVector<Member<AbstractRange>>* range_vector =
      MakeGarbageCollected<HeapVector<Member<AbstractRange>>>();
  range_vector->push_back(highlight_range_1);
  range_vector->push_back(highlight_range_2);
  Highlight* foo = Highlight::Create(*range_vector);
  Highlight* bar = Highlight::Create(*range_vector);
  registry->SetForTesting(AtomicString("foo"), foo);
  registry->SetForTesting(AtomicString("bar"), bar);
  registry->ScheduleRepaint();
  registry->ValidateHighlightMarkers();

  DocumentMarkerController& marker_controller = GetDocument().Markers();
  DocumentMarkerVector custom = marker_controller.MarkersFor(
      *text, DocumentMarker::MarkerTypes::CustomHighlight());
  EXPECT_EQ(
      HighlightOverlay::ComputeLayers(GetDocument(), text, style, text_style,
                                      paint_info, nullptr, custom, *none, *none,
                                      *none, *none),
      (HeapVector<HighlightLayer>{
          HighlightLayer{HighlightLayerType::kOriginating},
          HighlightLayer{HighlightLayerType::kCustom, AtomicString("foo")},
          HighlightLayer{HighlightLayerType::kCustom, AtomicString("bar")},
      }))
      << "should return kCustom layers no more than once each";
}

TEST_F(HighlightOverlayTest, ComputeEdges) {
  // #text "   foo" has two offset mapping units:
  // • DOM [0,3), text content [1,1)
  // • DOM [3,6), text content [1,4)
  SetBodyInnerHTML(R"HTML(<br>   foo<br>)HTML");
  Node* br = GetDocument().body()->firstChild();
  Node* text = br->nextSibling();
  UpdateAllLifecyclePhasesForTest();
  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);

  const ComputedStyle& br_style = br->GetLayoutObject()->StyleRef();
  const ComputedStyle& text_style = text->GetLayoutObject()->StyleRef();
  TextPaintStyle paint_style;

  TextOffsetRange originating{3, 6};
  LayoutSelectionStatus selection{1, 3, SelectSoftLineBreak::kNotSelected};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();

  HeapVector<HighlightLayer> layers;

  layers = HighlightOverlay::ComputeLayers(GetDocument(), text, text_style,
                                           paint_style, paint_info, nullptr,
                                           *none, *none, *none, *none, *none);
  EXPECT_EQ(
      HighlightOverlay::ComputeEdges(text, false, originating, layers, nullptr,
                                     *none, *none, *none, *none, *none),
      (Vector<HighlightEdge>{}))
      << "should return no edges when nothing is highlighted";

  layers = HighlightOverlay::ComputeLayers(GetDocument(), text, text_style,
                                           paint_style, paint_info, &selection,
                                           *none, *none, *none, *none, *none);
  EXPECT_EQ(HighlightOverlay::ComputeEdges(nullptr, false, originating, layers,
                                           &selection, *none, *none, *none,
                                           *none, *none),
            (Vector<HighlightEdge>{
                HighlightEdge{{1, 3},
                              HighlightLayerType::kSelection,
                              1,
                              HighlightEdgeType::kStart},
                HighlightEdge{{1, 3},
                              HighlightLayerType::kSelection,
                              1,
                              HighlightEdgeType::kEnd},
            }))
      << "should still return non-marker edges when node is nullptr";

  layers = HighlightOverlay::ComputeLayers(GetDocument(), br, br_style,
                                           paint_style, paint_info, &selection,
                                           *none, *none, *none, *none, *none);
  EXPECT_EQ(
      HighlightOverlay::ComputeEdges(br, false, originating, layers, &selection,
                                     *none, *none, *none, *none, *none),
      (Vector<HighlightEdge>{
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        1,
                        HighlightEdgeType::kStart},
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        1,
                        HighlightEdgeType::kEnd},
      }))
      << "should still return non-marker edges when node is <br>";

  auto* grammar = MakeGarbageCollected<DocumentMarkerVector>();
  auto* spelling = MakeGarbageCollected<DocumentMarkerVector>();
  auto* target = MakeGarbageCollected<DocumentMarkerVector>();
  auto* search = MakeGarbageCollected<DocumentMarkerVector>();
  grammar->push_back(MakeGarbageCollected<GrammarMarker>(3, 4, ""));
  grammar->push_back(MakeGarbageCollected<GrammarMarker>(4, 5, ""));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(4, 5));
  search->push_back(MakeGarbageCollected<TextMatchMarker>(
      4, 5, TextMatchMarker::MatchStatus::kActive));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(4, 5, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(5, 6, ""));

  layers = HighlightOverlay::ComputeLayers(
      GetDocument(), text, text_style, paint_style, paint_info, &selection,
      *none, *grammar, *spelling, *target, *search);
  EXPECT_EQ(
      HighlightOverlay::ComputeEdges(text, false, originating, layers,
                                     &selection, *none, *grammar, *spelling,
                                     *target, *search),
      (Vector<HighlightEdge>{
          HighlightEdge{{1, 2},
                        HighlightLayerType::kGrammar,
                        1,
                        HighlightEdgeType::kStart},
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        6,
                        HighlightEdgeType::kStart},
          HighlightEdge{
              {1, 2}, HighlightLayerType::kGrammar, 1, HighlightEdgeType::kEnd},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kGrammar,
                        1,
                        HighlightEdgeType::kStart},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kSpelling,
                        2,
                        HighlightEdgeType::kStart},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kTargetText,
                        3,
                        HighlightEdgeType::kStart},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kSearchTextCurrent,
                        5,
                        HighlightEdgeType::kStart},
          HighlightEdge{
              {2, 3}, HighlightLayerType::kGrammar, 1, HighlightEdgeType::kEnd},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kSpelling,
                        2,
                        HighlightEdgeType::kEnd},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kTargetText,
                        3,
                        HighlightEdgeType::kEnd},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kSearchTextCurrent,
                        5,
                        HighlightEdgeType::kEnd},
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        6,
                        HighlightEdgeType::kEnd},
          HighlightEdge{{3, 4},
                        HighlightLayerType::kSpelling,
                        2,
                        HighlightEdgeType::kStart},
          HighlightEdge{{3, 4},
                        HighlightLayerType::kSpelling,
                        2,
                        HighlightEdgeType::kEnd},
      }))
      << "should return edges in correct order";

  TextOffsetRange originating2{4, 5};

  EXPECT_EQ(
      HighlightOverlay::ComputeEdges(text, false, originating2, layers,
                                     &selection, *none, *grammar, *spelling,
                                     *target, *search),
      (Vector<HighlightEdge>{
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        6,
                        HighlightEdgeType::kStart},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kGrammar,
                        1,
                        HighlightEdgeType::kStart},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kSpelling,
                        2,
                        HighlightEdgeType::kStart},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kTargetText,
                        3,
                        HighlightEdgeType::kStart},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kSearchTextCurrent,
                        5,
                        HighlightEdgeType::kStart},
          HighlightEdge{
              {2, 3}, HighlightLayerType::kGrammar, 1, HighlightEdgeType::kEnd},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kSpelling,
                        2,
                        HighlightEdgeType::kEnd},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kTargetText,
                        3,
                        HighlightEdgeType::kEnd},
          HighlightEdge{{2, 3},
                        HighlightLayerType::kSearchTextCurrent,
                        5,
                        HighlightEdgeType::kEnd},
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        6,
                        HighlightEdgeType::kEnd},
      }))
      << "should skip edge pairs that are completely outside fragment";
}

TEST_F(HighlightOverlayTest, ComputeParts) {
  SetBodyInnerHTML(R"HTML(brown fxo oevr lazy dgo today)HTML");
  auto* text = DynamicTo<Text>(GetDocument().body()->firstChild());
  UpdateAllLifecyclePhasesForTest();

  PaintController controller;
  GraphicsContext context(controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground,
                       /*descendant_painting_blocked=*/false);

  const ComputedStyle& text_style = text->GetLayoutObject()->StyleRef();
  TextPaintStyle paint_style;

  auto* none = MakeGarbageCollected<DocumentMarkerVector>();
  auto* grammar = MakeGarbageCollected<DocumentMarkerVector>();
  auto* spelling = MakeGarbageCollected<DocumentMarkerVector>();
  auto* target = MakeGarbageCollected<DocumentMarkerVector>();

  HighlightRegistry* registry =
      HighlightRegistry::From(*text->GetDocument().domWindow());
  Range* foo_range = Range::Create(GetDocument());
  foo_range->setStart(text, 0);
  foo_range->setEnd(text, 14);
  Range* bar_range = Range::Create(GetDocument());
  bar_range->setStart(text, 10);
  bar_range->setEnd(text, 19);
  HeapVector<Member<AbstractRange>>* foo_range_vector =
      MakeGarbageCollected<HeapVector<Member<AbstractRange>>>();
  foo_range_vector->push_back(foo_range);
  HeapVector<Member<AbstractRange>>* bar_range_vector =
      MakeGarbageCollected<HeapVector<Member<AbstractRange>>>();
  bar_range_vector->push_back(bar_range);
  Highlight* foo = Highlight::Create(*foo_range_vector);
  Highlight* bar = Highlight::Create(*bar_range_vector);
  registry->SetForTesting(AtomicString("foo"), foo);
  registry->SetForTesting(AtomicString("bar"), bar);
  registry->ScheduleRepaint();
  registry->ValidateHighlightMarkers();
  DocumentMarkerController& marker_controller = GetDocument().Markers();
  DocumentMarkerVector custom = marker_controller.MarkersFor(
      *text, DocumentMarker::MarkerTypes::CustomHighlight());

  TextFragmentPaintInfo originating{"", 0, 25};
  TextOffsetRange originating_dom_offsets{0, 25};
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(6, 9, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(10, 14, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(20, 23, ""));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(15, 23));
  LayoutSelectionStatus selection{13, 19, SelectSoftLineBreak::kNotSelected};

  HeapVector<HighlightLayer> layers = HighlightOverlay::ComputeLayers(
      GetDocument(), text, text_style, paint_style, paint_info, &selection,
      custom, *grammar, *spelling, *target, *none);

  // Set up paint styles for each layer
  Color originating_color(0, 0, 0);
  Color originating_background(1, 0, 0);
  HighlightStyleUtils::HighlightColorPropertySet originating_current_colors;
  HighlightStyleUtils::HighlightTextPaintStyle originating_text_style{
      CreatePaintStyle(originating_color), originating_color,
      Color::kTransparent, originating_current_colors};
  layers[0].text_style = originating_text_style;

  Color foo_color(0, 0, 1);
  Color foo_background(1, 0, 1);
  HighlightStyleUtils::HighlightColorPropertySet foo_current_colors;
  HighlightStyleUtils::HighlightTextPaintStyle foo_text_style{
      CreatePaintStyle(foo_color), foo_color, foo_background,
      foo_current_colors};
  layers[1].text_style = foo_text_style;

  Color bar_color(0, 0, 2);
  Color bar_background(1, 0, 2);
  HighlightStyleUtils::HighlightColorPropertySet bar_current_colors{
      HighlightStyleUtils::HighlightColorProperty::kCurrentColor,
      HighlightStyleUtils::HighlightColorProperty::kFillColor,
      HighlightStyleUtils::HighlightColorProperty::kStrokeColor,
      HighlightStyleUtils::HighlightColorProperty::kEmphasisColor,
      HighlightStyleUtils::HighlightColorProperty::kSelectionDecorationColor,
      HighlightStyleUtils::HighlightColorProperty::kTextDecorationColor,
      HighlightStyleUtils::HighlightColorProperty::kBackgroundColor};
  HighlightStyleUtils::HighlightTextPaintStyle bar_text_style{
      CreatePaintStyle(bar_color), bar_color, bar_background,
      bar_current_colors};
  layers[2].text_style = bar_text_style;

  Color spelling_color(0, 0, 3);
  Color spelling_background(1, 0, 3);
  HighlightStyleUtils::HighlightColorPropertySet spelling_current_colors;
  HighlightStyleUtils::HighlightTextPaintStyle spelling_text_style{
      CreatePaintStyle(spelling_color), spelling_color, spelling_background,
      spelling_current_colors};
  layers[3].text_style = spelling_text_style;

  Color target_color(0, 0, 4);
  Color target_background(1, 0, 4);
  HighlightStyleUtils::HighlightColorPropertySet target_current_colors{
      HighlightStyleUtils::HighlightColorProperty::kCurrentColor,
      HighlightStyleUtils::HighlightColorProperty::kFillColor,
      HighlightStyleUtils::HighlightColorProperty::kStrokeColor,
      HighlightStyleUtils::HighlightColorProperty::kEmphasisColor,
      HighlightStyleUtils::HighlightColorProperty::kSelectionDecorationColor,
      HighlightStyleUtils::HighlightColorProperty::kTextDecorationColor,
      HighlightStyleUtils::HighlightColorProperty::kBackgroundColor};
  HighlightStyleUtils::HighlightTextPaintStyle target_text_style{
      CreatePaintStyle(target_color), target_color, target_background,
      target_current_colors};
  layers[4].text_style = target_text_style;

  Color selection_color(0, 0, 5);
  Color selection_background(1, 0, 5);
  HighlightStyleUtils::HighlightColorPropertySet selection_current_colors;
  HighlightStyleUtils::HighlightTextPaintStyle selection_text_style{
      CreatePaintStyle(selection_color), selection_color, selection_background,
      selection_current_colors};
  layers[5].text_style = selection_text_style;

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  // [                       ]        originating
  //                                  ::highlight(foo), not active
  //                                  ::highlight(bar), not active
  //                                  ::spelling-error, not active
  //                                  ::target-text, not active
  //                                  ::selection, not active
  //                                  ::search-text, not active

  Vector<HighlightEdge> edges = HighlightOverlay::ComputeEdges(
      text, false, originating_dom_offsets, layers, nullptr, *none, *none,
      *none, *none, *none);

  // clang-format off
  EXPECT_EQ(HighlightOverlay::ComputeParts(originating, layers, edges),
            (HeapVector<HighlightPart>{
                HighlightPart{HighlightLayerType::kOriginating, 0, {0,25}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color}}},
            }))
      << "should return correct kOriginating part when nothing is highlighted";

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  // [                       ]        originating, as above
  // [            ]                   ::highlight(foo), changed!
  //           [       ]              ::highlight(bar), changed!
  //       [ ] [  ]      [ ]          ::spelling-error, changed!
  //                [      ]          ::target-text, changed!
  //              [    ]              ::selection, changed!
  //                                  ::search-text, not active

  Vector<HighlightEdge> edges2 = HighlightOverlay::ComputeEdges(
      text, false, originating_dom_offsets, layers, &selection, custom,
      *grammar, *spelling, *target, *none);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating, layers, edges2),
            (HeapVector<HighlightPart>{
                HighlightPart{HighlightLayerType::kCustom, 1, {0,6}, foo_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {0,14}, foo_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {6,9}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {0,14}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {6,9}, spelling_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kSpelling, 3, spelling_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color}}},
                HighlightPart{HighlightLayerType::kCustom, 1, {9,10}, foo_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {0,14}, foo_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {10,13}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {0,14}, foo_color},
                               {HighlightLayerType::kCustom, 2, {10,19}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {10,14}, spelling_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {13,14}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {0,14}, foo_color},
                               {HighlightLayerType::kCustom, 2, {10,19}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {10,14}, spelling_color},
                               {HighlightLayerType::kSelection, 5, {13,19}, selection_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_background},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {14,15}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 2, {10,19}, originating_color},
                               {HighlightLayerType::kSelection, 5, {13,19}, selection_color}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {15,19}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 2, {10,19}, originating_color},
                               {HighlightLayerType::kTargetText, 4, {15,23}, originating_color},
                               {HighlightLayerType::kSelection, 5, {13,19}, selection_color}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kTargetText, 4, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kTargetText, 4, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
                HighlightPart{HighlightLayerType::kTargetText, 4, {19,20}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kTargetText, 4, {15,23}, originating_color}},
                              {{HighlightLayerType::kTargetText, 4, originating_color}},
                              {{HighlightLayerType::kTargetText, 4, originating_color}}},
                HighlightPart{HighlightLayerType::kTargetText, 4, {20,23}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kSpelling, 3, {20,23}, spelling_color},
                               {HighlightLayerType::kTargetText, 4, {15,23}, spelling_color}},
                              {{HighlightLayerType::kSpelling, 3, spelling_background},
                               {HighlightLayerType::kTargetText, 4, spelling_color}},
                              {{HighlightLayerType::kSpelling, 3, spelling_color},
                               {HighlightLayerType::kTargetText, 4, spelling_color}}},
                HighlightPart{HighlightLayerType::kOriginating, 0, {23,25}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color}}},
            }))
      << "should return correct parts given several active highlights";

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  // [                       ]        originating, as above
  //       [      ]                   ::highlight(foo), changed!
  //           [       ]              ::highlight(bar), as above
  //       [ ] [  ]      [ ]          ::spelling-error, as above
  //                [      ]          ::target-text, as above
  //              [    ]              ::selection, as above
  //                                  ::search-text, not active

  foo_range->setStart(text, 6);
  registry->ScheduleRepaint();
  registry->ValidateHighlightMarkers();
  custom = marker_controller.MarkersFor(*text, DocumentMarker::MarkerTypes::CustomHighlight());
  Vector<HighlightEdge> edges3 = HighlightOverlay::ComputeEdges(
      text, false, originating_dom_offsets, layers, &selection, custom,
      *grammar, *spelling, *target, *none);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating, layers, edges3),
            (HeapVector<HighlightPart>{
                HighlightPart{HighlightLayerType::kOriginating, 0, {0,6}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {6,9}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {6,14}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {6,9}, spelling_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kSpelling, 3, spelling_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color}}},
                HighlightPart{HighlightLayerType::kCustom, 1, {9,10}, foo_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {6,14}, foo_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {10,13}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {6,14}, foo_color},
                               {HighlightLayerType::kCustom, 2, {10,19}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {10,14}, spelling_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {13,14}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 1, {6,14}, foo_color},
                               {HighlightLayerType::kCustom, 2, {10,19}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {10,14}, spelling_color},
                               {HighlightLayerType::kSelection, 5, {13,19}, selection_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_background},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {14,15}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 2, {10,19}, originating_color},
                               {HighlightLayerType::kSelection, 5, {13,19}, selection_color}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {15,19}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kCustom, 2, {10,19}, originating_color},
                               {HighlightLayerType::kTargetText, 4, {15,23}, originating_color},
                               {HighlightLayerType::kSelection, 5, {13,19}, selection_color}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kTargetText, 4, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kTargetText, 4, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
                HighlightPart{HighlightLayerType::kTargetText, 4, {19,20}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kTargetText, 4, {15,23}, originating_color}},
                              {{HighlightLayerType::kTargetText, 4, originating_color}},
                              {{HighlightLayerType::kTargetText, 4, originating_color}}},
                HighlightPart{HighlightLayerType::kTargetText, 4, {20,23}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color},
                               {HighlightLayerType::kSpelling, 3, {20,23}, spelling_color},
                               {HighlightLayerType::kTargetText, 4, {15,23}, spelling_color}},
                              {{HighlightLayerType::kSpelling, 3, spelling_background},
                               {HighlightLayerType::kTargetText, 4, spelling_color}},
                              {{HighlightLayerType::kSpelling, 3, spelling_color},
                               {HighlightLayerType::kTargetText, 4, spelling_color}}},
                HighlightPart{HighlightLayerType::kOriginating, 0, {23,25}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {0,25}, originating_color}}},
            }))
      << "correct when first edge starts after start of originating fragment";

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  //         [        ]               originating, changed!
  //       [      ]                   ::highlight(foo), as above
  //           [       ]              ::highlight(bar), as above
  //       [ ] [  ]      [ ]          ::spelling-error, as above
  //                [      ]          ::target-text, as above
  //              [    ]              ::selection, as above
  //                                  ::search-text, not active

  TextFragmentPaintInfo originating2{"", 8, 18};
  TextOffsetRange originating2_dom_offsets{8, 18};
  Vector<HighlightEdge> edges4 = HighlightOverlay::ComputeEdges(
      text, false, originating2_dom_offsets, layers, &selection, custom,
      *grammar, *spelling, *target, *none);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating2, layers, edges4),
            (HeapVector<HighlightPart>{
                HighlightPart{HighlightLayerType::kSpelling, 3, {8,9}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color},
                               {HighlightLayerType::kCustom, 1, {8,14}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {8,9}, spelling_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kSpelling, 3, spelling_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color}}},
                HighlightPart{HighlightLayerType::kCustom, 1, {9,10}, foo_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color},
                               {HighlightLayerType::kCustom, 1, {8,14}, foo_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {10,13}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color},
                               {HighlightLayerType::kCustom, 1, {8,14}, foo_color},
                               {HighlightLayerType::kCustom, 2, {10,18}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {10,14}, spelling_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {13,14}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color},
                               {HighlightLayerType::kCustom, 1, {8,14}, foo_color},
                               {HighlightLayerType::kCustom, 2, {10,18}, foo_color},
                               {HighlightLayerType::kSpelling, 3, {10,14}, spelling_color},
                               {HighlightLayerType::kSelection, 5, {13,18}, selection_color}},
                              {{HighlightLayerType::kCustom, 1, foo_background},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_background},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 1, foo_color},
                               {HighlightLayerType::kCustom, 2, foo_color},
                               {HighlightLayerType::kSpelling, 3, spelling_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {14,15}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color},
                               {HighlightLayerType::kCustom, 2, {10,18}, originating_color},
                               {HighlightLayerType::kSelection, 5, {13,18}, selection_color}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {15,18}, selection_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color},
                               {HighlightLayerType::kCustom, 2, {10,18}, originating_color},
                               {HighlightLayerType::kTargetText, 4, {15,18}, originating_color},
                               {HighlightLayerType::kSelection, 5, {13,18}, selection_color}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kTargetText, 4, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_background}},
                              {{HighlightLayerType::kCustom, 2, originating_color},
                               {HighlightLayerType::kTargetText, 4, originating_color},
                               {HighlightLayerType::kSelection, 5, selection_color}}},
            }))
      << "should clamp result to originating fragment offsets";

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  //         [        ]               originating, as above
  //                                  ::highlight(foo), changed!
  //                                  ::highlight(bar), changed!
  //       [ ] [  ]      [ ]          ::spelling-error, as above
  //                                  ::target-text, changed!
  //                                  ::selection, changed!
  //                                  ::search-text, not active

  Vector<HighlightEdge> edges5 = HighlightOverlay::ComputeEdges(
      text, false, originating2_dom_offsets, layers, nullptr, *none, *none,
      *spelling, *none, *none);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating2, layers, edges5),
            (HeapVector<HighlightPart>{
                HighlightPart{HighlightLayerType::kSpelling, 3, {8,9}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color},
                               {HighlightLayerType::kSpelling, 3, {8,9}, spelling_color}},
                              {{HighlightLayerType::kSpelling, 3, spelling_background}},
                              {{HighlightLayerType::kSpelling, 3, spelling_color}}},
                HighlightPart{HighlightLayerType::kOriginating, 0, {9,10}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {10,14}, spelling_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color},
                               {HighlightLayerType::kSpelling, 3, {10,14}, spelling_color}},
                              {{HighlightLayerType::kSpelling, 3, spelling_background}},
                              {{HighlightLayerType::kSpelling, 3, spelling_color}}},
                HighlightPart{HighlightLayerType::kOriginating, 0, {14,18}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {8,18}, originating_color}}},
            }))
      << "should not crash if there is a gap in active layers";

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  //  [ ]                             originating, changed!
  //                                  ::highlight(foo), as above
  //                                  ::highlight(bar), as above
  //       [ ] [  ]      [ ]          ::spelling-error, as above
  //                                  ::target-text, as above
  //                                  ::selection, as above
  //                                  ::search-text, not active

  TextFragmentPaintInfo originating3{"", 1, 4};
  TextOffsetRange originating3_dom_offsets{1, 4};
  Vector<HighlightEdge> edges6 = HighlightOverlay::ComputeEdges(
      text, false, originating3_dom_offsets, layers, &selection, custom,
      *grammar, *spelling, *target, *none);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating3, layers, edges6),
            (HeapVector<HighlightPart>{
                HighlightPart{HighlightLayerType::kOriginating, 0, {1,4}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {1,4}, originating_color}}},
            }))
      << "correct when first edge starts after end of originating fragment";

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  //                          [ ]     originating, changed!
  //                                  ::highlight(foo), as above
  //                                  ::highlight(bar), as above
  //       [ ] [  ]      [ ]          ::spelling-error, as above
  //                                  ::target-text, as above
  //                                  ::selection, as above
  //                                  ::search-text, not active

  TextFragmentPaintInfo originating4{"", 25, 28};
  TextOffsetRange originating4_dom_offsets{25, 28};
  Vector<HighlightEdge> edges7 = HighlightOverlay::ComputeEdges(
      text, false, originating4_dom_offsets, layers, &selection, custom,
      *grammar, *spelling, *target, *none);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating4, layers, edges7),
            (HeapVector<HighlightPart>{
                HighlightPart{HighlightLayerType::kOriginating, 0, {25,28}, originating_text_style.style, 0,
                              {{HighlightLayerType::kOriginating, 0, {25,28}, originating_color}}},
            }))
      << "correct when last edge ends before start of originating fragment";
}

}  // namespace blink
