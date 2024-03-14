// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_overlay.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
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
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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
  HighlightOverlayTest() : PageTestBase() {}
};

TEST_F(HighlightOverlayTest, ComputeLayers) {
  SetBodyInnerHTML(R"HTML(foo)HTML");
  Node* text = GetDocument().body()->firstChild();
  UpdateAllLifecyclePhasesForTest();

  LayoutSelectionStatus selection{0, 0, SelectSoftLineBreak::kNotSelected};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();
  const ComputedStyle& style = text->GetLayoutObject()->StyleRef();
  TextPaintStyle text_style;
  PaintController* controller = MakeGarbageCollected<PaintController>();
  GraphicsContext context(*controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground);

  EXPECT_EQ(HighlightOverlay::ComputeLayers(GetDocument(), text, style,
                                            text_style, paint_info, nullptr,
                                            *none, *none, *none, *none),
            HeapVector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating}})
      << "should return kOriginating when nothing is highlighted";

  EXPECT_EQ(HighlightOverlay::ComputeLayers(GetDocument(), text, style,
                                            text_style, paint_info, &selection,
                                            *none, *none, *none, *none),
            (HeapVector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating},
                HighlightLayer{HighlightLayerType::kSelection},
            }))
      << "should return kSelection when a selection is given";

  auto* grammar = MakeGarbageCollected<DocumentMarkerVector>();
  auto* spelling = MakeGarbageCollected<DocumentMarkerVector>();
  auto* target = MakeGarbageCollected<DocumentMarkerVector>();
  grammar->push_back(MakeGarbageCollected<GrammarMarker>(0, 1, ""));
  grammar->push_back(MakeGarbageCollected<GrammarMarker>(0, 1, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(0, 1, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(0, 1, ""));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(0, 1));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(0, 1));

  EXPECT_EQ(HighlightOverlay::ComputeLayers(
                GetDocument(), text, style, text_style, paint_info, nullptr,
                *none, *grammar, *spelling, *target),
            (HeapVector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating},
                HighlightLayer{HighlightLayerType::kGrammar},
                HighlightLayer{HighlightLayerType::kSpelling},
                HighlightLayer{HighlightLayerType::kTargetText},
            }))
      << "should return kGrammar + kSpelling + kTargetText no more than once "
         "each";

  HighlightRegistry* registry =
      HighlightRegistry::From(*text->GetDocument().domWindow());
  Highlight* foo = Highlight::Create(
      *MakeGarbageCollected<HeapVector<Member<AbstractRange>>>());
  Highlight* bar = Highlight::Create(
      *MakeGarbageCollected<HeapVector<Member<AbstractRange>>>());
  registry->SetForTesting(AtomicString("foo"), foo);
  registry->SetForTesting(AtomicString("bar"), bar);

  auto* custom = MakeGarbageCollected<DocumentMarkerVector>();
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 1, "bar", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 1, "foo", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 1, "bar", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 1, "foo", nullptr));

  EXPECT_EQ(
      HighlightOverlay::ComputeLayers(GetDocument(), text, style, text_style,
                                      paint_info, nullptr, *custom, *none,
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
  PaintController* controller = MakeGarbageCollected<PaintController>();
  GraphicsContext context(*controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground);

  const ComputedStyle& br_style = br->GetLayoutObject()->StyleRef();
  const ComputedStyle& text_style = text->GetLayoutObject()->StyleRef();
  TextPaintStyle paint_style;

  TextOffsetRange originating{3, 6};
  LayoutSelectionStatus selection{1, 3, SelectSoftLineBreak::kNotSelected};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();

  HeapVector<HighlightLayer> layers;

  layers = HighlightOverlay::ComputeLayers(GetDocument(), text, text_style,
                                           paint_style, paint_info, nullptr,
                                           *none, *none, *none, *none);
  EXPECT_EQ(HighlightOverlay::ComputeEdges(text, false, originating, layers,
                                           nullptr, *none, *none, *none, *none),
            (Vector<HighlightEdge>{}))
      << "should return no edges when nothing is highlighted";

  layers = HighlightOverlay::ComputeLayers(GetDocument(), text, text_style,
                                           paint_style, paint_info, &selection,
                                           *none, *none, *none, *none);
  EXPECT_EQ(
      HighlightOverlay::ComputeEdges(nullptr, false, originating, layers,
                                     &selection, *none, *none, *none, *none),
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
                                           *none, *none, *none, *none);
  EXPECT_EQ(
      HighlightOverlay::ComputeEdges(br, false, originating, layers, &selection,
                                     *none, *none, *none, *none),
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
  grammar->push_back(MakeGarbageCollected<GrammarMarker>(3, 4, ""));
  grammar->push_back(MakeGarbageCollected<GrammarMarker>(4, 5, ""));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(4, 5));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(4, 5, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(5, 6, ""));

  layers = HighlightOverlay::ComputeLayers(GetDocument(), text, text_style,
                                           paint_style, paint_info, &selection,
                                           *none, *grammar, *spelling, *target);
  EXPECT_EQ(
      HighlightOverlay::ComputeEdges(text, false, originating, layers,
                                     &selection, *none, *grammar, *spelling,
                                     *target),
      (Vector<HighlightEdge>{
          HighlightEdge{{1, 2},
                        HighlightLayerType::kGrammar,
                        1,
                        HighlightEdgeType::kStart},
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        4,
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
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        4,
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
                                     *target),
      (Vector<HighlightEdge>{
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        4,
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
          HighlightEdge{{1, 3},
                        HighlightLayerType::kSelection,
                        4,
                        HighlightEdgeType::kEnd},
      }))
      << "should skip edge pairs that are completely outside fragment";
}

TEST_F(HighlightOverlayTest, ComputeParts) {
  SetBodyInnerHTML(R"HTML(brown fxo oevr lazy dgo today)HTML");
  Node* node = GetDocument().body()->firstChild();
  UpdateAllLifecyclePhasesForTest();

  PaintController* controller = MakeGarbageCollected<PaintController>();
  GraphicsContext context(*controller);
  PaintInfo paint_info(context, CullRect(), PaintPhase::kForeground);

  const ComputedStyle& text_style = node->GetLayoutObject()->StyleRef();
  TextPaintStyle paint_style;

  HighlightRegistry* registry =
      HighlightRegistry::From(*node->GetDocument().domWindow());
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();
  auto* custom = MakeGarbageCollected<DocumentMarkerVector>();
  auto* grammar = MakeGarbageCollected<DocumentMarkerVector>();
  auto* spelling = MakeGarbageCollected<DocumentMarkerVector>();
  auto* target = MakeGarbageCollected<DocumentMarkerVector>();
  Highlight* foo_highlight = Highlight::Create(
      *MakeGarbageCollected<HeapVector<Member<AbstractRange>>>());
  Highlight* bar_highlight = Highlight::Create(
      *MakeGarbageCollected<HeapVector<Member<AbstractRange>>>());
  registry->SetForTesting(AtomicString("foo"), foo_highlight);
  registry->SetForTesting(AtomicString("bar"), bar_highlight);

  TextFragmentPaintInfo originating{"", 0, 25};
  TextOffsetRange originating_dom_offsets{0, 25};
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 14, "foo", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(10, 19, "bar", nullptr));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(6, 9, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(10, 14, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(20, 23, ""));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(15, 23));
  LayoutSelectionStatus selection{13, 19, SelectSoftLineBreak::kNotSelected};

  HeapVector<HighlightLayer> layers = HighlightOverlay::ComputeLayers(
      GetDocument(), node, text_style, paint_style, paint_info, &selection,
      *custom, *grammar, *spelling, *target);

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  // [                       ]        originating
  //                                  ::highlight(foo), not active
  //                                  ::highlight(bar), not active
  //                                  ::spelling-error, not active
  //                                  ::target-text, not active
  //                                  ::selection, not active

  Vector<HighlightEdge> edges = HighlightOverlay::ComputeEdges(
      node, false, originating_dom_offsets, layers, nullptr, *none, *none,
      *none, *none);

  // clang-format off
  EXPECT_EQ(HighlightOverlay::ComputeParts(originating, layers, edges),
            (Vector<HighlightPart>{
                HighlightPart{HighlightLayerType::kOriginating, 0, {0,25},
                              {{HighlightLayerType::kOriginating, 0, {0,25}}}},
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

  Vector<HighlightEdge> edges2 = HighlightOverlay::ComputeEdges(
      node, false, originating_dom_offsets, layers, &selection, *custom,
      *grammar, *spelling, *target);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating, layers, edges2),
            (Vector<HighlightPart>{
                HighlightPart{HighlightLayerType::kCustom, 1, {0,6},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1, {0,14}}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {6,9},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1,{ 0,14}},
                               {HighlightLayerType::kSpelling, 3, {6,9}}}},
                HighlightPart{HighlightLayerType::kCustom, 1, {9,10},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1, {0,14}}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {10,13},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1, {0,14}},
                               {HighlightLayerType::kCustom, 2, {10,19}},
                               {HighlightLayerType::kSpelling, 3, {10,14}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {13,14},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1, {0,14}},
                               {HighlightLayerType::kCustom, 2, {10,19}},
                               {HighlightLayerType::kSpelling, 3, {10,14}},
                               {HighlightLayerType::kSelection, 5, {13,19}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {14,15},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 2, {10,19}},
                               {HighlightLayerType::kSelection, 5, {13,19}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {15,19},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 2, {10,19}},
                               {HighlightLayerType::kTargetText, 4, {15,23}},
                               {HighlightLayerType::kSelection, 5, {13,19}}}},
                HighlightPart{HighlightLayerType::kTargetText, 4, {19,20},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kTargetText, 4, {15,23}}}},
                HighlightPart{HighlightLayerType::kTargetText, 4, {20,23},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kSpelling, 3, {20,23}},
                               {HighlightLayerType::kTargetText, 4, {15,23}}}},
                HighlightPart{HighlightLayerType::kOriginating, 0, {23,25},
                              {{HighlightLayerType::kOriginating, 0, {0,25}}}},
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

  custom->at(0)->SetStartOffset(6);
  Vector<HighlightEdge> edges3 = HighlightOverlay::ComputeEdges(
      node, false, originating_dom_offsets, layers, &selection, *custom,
      *grammar, *spelling, *target);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating, layers, edges3),
            (Vector<HighlightPart>{
                HighlightPart{HighlightLayerType::kOriginating, 0, {0,6},
                              {{HighlightLayerType::kOriginating, 0, {0,25}}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {6,9},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1, {6,14}},
                               {HighlightLayerType::kSpelling, 3, {6,9}}}},
                HighlightPart{HighlightLayerType::kCustom, 1, {9,10},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1, {6,14}}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {10,13},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1, {6,14}},
                               {HighlightLayerType::kCustom, 2, {10,19}},
                               {HighlightLayerType::kSpelling, 3, {10,14}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {13,14},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 1, {6,14}},
                               {HighlightLayerType::kCustom, 2, {10,19}},
                               {HighlightLayerType::kSpelling, 3, {10,14}},
                               {HighlightLayerType::kSelection, 5, {13,19}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {14,15},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 2, {10,19}},
                               {HighlightLayerType::kSelection, 5, {13,19}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {15,19},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kCustom, 2, {10,19}},
                               {HighlightLayerType::kTargetText, 4, {15,23}},
                               {HighlightLayerType::kSelection, 5, {13,19}}}},
                HighlightPart{HighlightLayerType::kTargetText, 4, {19,20},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kTargetText, 4, {15,23}}}},
                HighlightPart{HighlightLayerType::kTargetText, 4, {20,23},
                              {{HighlightLayerType::kOriginating, 0, {0,25}},
                               {HighlightLayerType::kSpelling, 3, {20,23}},
                               {HighlightLayerType::kTargetText, 4,{15,23}}}},
                HighlightPart{HighlightLayerType::kOriginating, 0, {23,25},
                              {{HighlightLayerType::kOriginating, 0, {0,25}}}},
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

  TextFragmentPaintInfo originating2{"", 8, 18};
  TextOffsetRange originating2_dom_offsets{8, 18};
  Vector<HighlightEdge> edges4 = HighlightOverlay::ComputeEdges(
      node, false, originating2_dom_offsets, layers, &selection, *custom,
      *grammar, *spelling, *target);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating2, layers, edges4),
            (Vector<HighlightPart>{
                HighlightPart{HighlightLayerType::kSpelling, 3, {8,9},
                              {{HighlightLayerType::kOriginating, 0, {8,18}},
                               {HighlightLayerType::kCustom, 1, {8,14}},
                               {HighlightLayerType::kSpelling, 3, {8,9}}}},
                HighlightPart{HighlightLayerType::kCustom, 1, {9,10},
                              {{HighlightLayerType::kOriginating, 0, {8,18}},
                               {HighlightLayerType::kCustom, 1, {8,14}}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {10,13},
                              {{HighlightLayerType::kOriginating, 0, {8,18}},
                               {HighlightLayerType::kCustom, 1, {8,14}},
                               {HighlightLayerType::kCustom, 2, {10,18}},
                               {HighlightLayerType::kSpelling, 3, {10,14}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {13,14},
                              {{HighlightLayerType::kOriginating, 0, {8,18}},
                               {HighlightLayerType::kCustom, 1, {8,14}},
                               {HighlightLayerType::kCustom, 2, {10,18}},
                               {HighlightLayerType::kSpelling, 3, {10,14}},
                               {HighlightLayerType::kSelection, 5, {13,18}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {14,15},
                              {{HighlightLayerType::kOriginating, 0, {8,18}},
                               {HighlightLayerType::kCustom, 2, {10,18}},
                               {HighlightLayerType::kSelection, 5, {13,18}}}},
                HighlightPart{HighlightLayerType::kSelection, 5, {15,18},
                              {{HighlightLayerType::kOriginating, 0, {8,18}},
                               {HighlightLayerType::kCustom, 2, {10,18}},
                               {HighlightLayerType::kTargetText, 4, {15,18}},
                               {HighlightLayerType::kSelection, 5, {13,18}}}},
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

  Vector<HighlightEdge> edges5 = HighlightOverlay::ComputeEdges(
      node, false, originating2_dom_offsets, layers, nullptr, *none, *none,
      *spelling, *none);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating2, layers, edges5),
            (Vector<HighlightPart>{
                HighlightPart{HighlightLayerType::kSpelling, 3, {8,9},
                              {{HighlightLayerType::kOriginating, 0, {8,18}},
                               {HighlightLayerType::kSpelling, 3, {8,9}}}},
                HighlightPart{HighlightLayerType::kOriginating, 0, {9,10},
                              {{HighlightLayerType::kOriginating, 0, {8,18}}}},
                HighlightPart{HighlightLayerType::kSpelling, 3, {10,14},
                              {{HighlightLayerType::kOriginating, 0, {8,18}},
                               {HighlightLayerType::kSpelling, 3, {10,14}}}},
                HighlightPart{HighlightLayerType::kOriginating, 0, {14,18},
                              {{HighlightLayerType::kOriginating, 0, {8,18}}}},
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

  TextFragmentPaintInfo originating3{"", 1, 4};
  TextOffsetRange originating3_dom_offsets{1, 4};
  Vector<HighlightEdge> edges6 = HighlightOverlay::ComputeEdges(
      node, false, originating3_dom_offsets, layers, &selection, *custom,
      *grammar, *spelling, *target);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating3, layers, edges6),
            (Vector<HighlightPart>{
                HighlightPart{HighlightLayerType::kOriginating, 0, {1,4},
                              {{HighlightLayerType::kOriginating, 0, {1,4}}}},
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

  TextFragmentPaintInfo originating4{"", 25, 28};
  TextOffsetRange originating4_dom_offsets{25, 28};
  Vector<HighlightEdge> edges7 = HighlightOverlay::ComputeEdges(
      node, false, originating4_dom_offsets, layers, &selection, *custom,
      *grammar, *spelling, *target);

  EXPECT_EQ(HighlightOverlay::ComputeParts(originating4, layers, edges7),
            (Vector<HighlightPart>{
                HighlightPart{HighlightLayerType::kOriginating, 0, {25,28},
                              {{HighlightLayerType::kOriginating, 0, {25,28}}}},
            }))
      << "correct when last edge ends before start of originating fragment";
}

}  // namespace blink
