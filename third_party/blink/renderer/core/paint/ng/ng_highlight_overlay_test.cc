// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_highlight_overlay.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/custom_highlight_marker.h"
#include "third_party/blink/renderer/core/editing/markers/grammar_marker.h"
#include "third_party/blink/renderer/core/editing/markers/spelling_marker.h"
#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

using HighlightLayerType = NGHighlightOverlay::HighlightLayerType;
using HighlightEdgeType = NGHighlightOverlay::HighlightEdgeType;
using HighlightLayer = NGHighlightOverlay::HighlightLayer;
using HighlightEdge = NGHighlightOverlay::HighlightEdge;
using HighlightPart = NGHighlightOverlay::HighlightPart;

}  // namespace

class NGHighlightOverlayTest : public PageTestBase,
                               private ScopedHighlightOverlayPaintingForTest {
 public:
  NGHighlightOverlayTest() : ScopedHighlightOverlayPaintingForTest(true) {}
};

TEST_F(NGHighlightOverlayTest, ComputeLayers) {
  auto* registry =
      MakeGarbageCollected<HighlightRegistry>(*GetFrame().DomWindow());
  LayoutSelectionStatus selection{0, 0, SelectSoftLineBreak::kNotSelected};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();

  EXPECT_EQ(
      NGHighlightOverlay::ComputeLayers(registry, nullptr, *none, *none, *none,
                                        *none),
      Vector<HighlightLayer>{HighlightLayer{HighlightLayerType::kOriginating}})
      << "should return kOriginating when nothing is highlighted";

  EXPECT_EQ(NGHighlightOverlay::ComputeLayers(registry, &selection, *none,
                                              *none, *none, *none),
            (Vector<HighlightLayer>{
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

  EXPECT_EQ(NGHighlightOverlay::ComputeLayers(registry, nullptr, *none,
                                              *grammar, *spelling, *target),
            (Vector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating},
                HighlightLayer{HighlightLayerType::kGrammar},
                HighlightLayer{HighlightLayerType::kSpelling},
                HighlightLayer{HighlightLayerType::kTargetText},
            }))
      << "should return kGrammar + kSpelling + kTargetText no more than once "
         "each";

  Highlight* foo = Highlight::Create(
      *MakeGarbageCollected<HeapVector<Member<AbstractRange>>>());
  Highlight* bar = Highlight::Create(
      *MakeGarbageCollected<HeapVector<Member<AbstractRange>>>());
  registry->SetForTesting("foo", foo);
  registry->SetForTesting("bar", bar);

  auto* custom = MakeGarbageCollected<DocumentMarkerVector>();
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 1, "bar", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 1, "foo", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 1, "bar", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 1, "foo", nullptr));

  EXPECT_EQ(NGHighlightOverlay::ComputeLayers(registry, nullptr, *custom, *none,
                                              *none, *none),
            (Vector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating},
                HighlightLayer{HighlightLayerType::kCustom, "foo"},
                HighlightLayer{HighlightLayerType::kCustom, "bar"},
            }))
      << "should return kCustom layers no more than once each";
}

TEST_F(NGHighlightOverlayTest, ComputeEdges) {
  // #text "   foo" has two offset mapping units:
  // • DOM [0,3), text content [1,1)
  // • DOM [3,6), text content [1,4)
  SetBodyInnerHTML(R"HTML(<br>   foo<br>)HTML");
  const Node* br = GetDocument().body()->childNodes()->item(0);
  const Node* text = GetDocument().body()->childNodes()->item(1);
  UpdateAllLifecyclePhasesForTest();

  auto* registry =
      MakeGarbageCollected<HighlightRegistry>(*GetFrame().DomWindow());
  NGTextFragmentPaintInfo originating{"", 1, 4};
  LayoutSelectionStatus selection{1, 3, SelectSoftLineBreak::kNotSelected};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();
  HighlightLayer originating_layer{HighlightLayerType::kOriginating};
  HighlightLayer selection_layer{HighlightLayerType::kSelection};
  HighlightLayer grammar_layer{HighlightLayerType::kGrammar};
  HighlightLayer spelling_layer{HighlightLayerType::kSpelling};
  HighlightLayer target_layer{HighlightLayerType::kTargetText};

  EXPECT_EQ(
      NGHighlightOverlay::ComputeEdges(text, registry, false, originating,
                                       nullptr, *none, *none, *none, *none),
      (Vector<HighlightEdge>{}))
      << "should return no edges when nothing is highlighted";

  EXPECT_EQ(
      NGHighlightOverlay::ComputeEdges(nullptr, registry, false, originating,
                                       &selection, *none, *none, *none, *none),
      (Vector<HighlightEdge>{
          HighlightEdge{1, selection_layer, HighlightEdgeType::kStart},
          HighlightEdge{3, selection_layer, HighlightEdgeType::kEnd},
      }))
      << "should still return non-marker edges when node is nullptr";

  EXPECT_EQ(
      NGHighlightOverlay::ComputeEdges(br, registry, false, originating,
                                       &selection, *none, *none, *none, *none),
      (Vector<HighlightEdge>{
          HighlightEdge{1, selection_layer, HighlightEdgeType::kStart},
          HighlightEdge{3, selection_layer, HighlightEdgeType::kEnd},
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

  EXPECT_EQ(NGHighlightOverlay::ComputeEdges(text, registry, false, originating,
                                             &selection, *none, *grammar,
                                             *spelling, *target),
            (Vector<HighlightEdge>{
                HighlightEdge{1, grammar_layer, HighlightEdgeType::kStart},
                HighlightEdge{1, selection_layer, HighlightEdgeType::kStart},
                HighlightEdge{2, grammar_layer, HighlightEdgeType::kEnd},
                HighlightEdge{2, grammar_layer, HighlightEdgeType::kStart},
                HighlightEdge{2, spelling_layer, HighlightEdgeType::kStart},
                HighlightEdge{2, target_layer, HighlightEdgeType::kStart},
                HighlightEdge{3, grammar_layer, HighlightEdgeType::kEnd},
                HighlightEdge{3, spelling_layer, HighlightEdgeType::kEnd},
                HighlightEdge{3, target_layer, HighlightEdgeType::kEnd},
                HighlightEdge{3, selection_layer, HighlightEdgeType::kEnd},
                HighlightEdge{3, spelling_layer, HighlightEdgeType::kStart},
                HighlightEdge{4, spelling_layer, HighlightEdgeType::kEnd},
            }))
      << "should return edges in correct order";

  NGTextFragmentPaintInfo originating2{"", 2, 3};

  EXPECT_EQ(NGHighlightOverlay::ComputeEdges(text, registry, false,
                                             originating2, &selection, *none,
                                             *grammar, *spelling, *target),
            (Vector<HighlightEdge>{
                HighlightEdge{1, selection_layer, HighlightEdgeType::kStart},
                HighlightEdge{2, grammar_layer, HighlightEdgeType::kStart},
                HighlightEdge{2, spelling_layer, HighlightEdgeType::kStart},
                HighlightEdge{2, target_layer, HighlightEdgeType::kStart},
                HighlightEdge{3, grammar_layer, HighlightEdgeType::kEnd},
                HighlightEdge{3, spelling_layer, HighlightEdgeType::kEnd},
                HighlightEdge{3, target_layer, HighlightEdgeType::kEnd},
                HighlightEdge{3, selection_layer, HighlightEdgeType::kEnd},
            }))
      << "should skip edge pairs that are completely outside fragment";
}

TEST_F(NGHighlightOverlayTest, ComputeParts) {
  SetBodyInnerHTML(R"HTML(brown fxo oevr lazy dgo today)HTML");
  const Node* node = GetDocument().body()->childNodes()->item(0);
  UpdateAllLifecyclePhasesForTest();

  auto* registry =
      MakeGarbageCollected<HighlightRegistry>(*GetFrame().DomWindow());
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();
  auto* custom = MakeGarbageCollected<DocumentMarkerVector>();
  auto* grammar = MakeGarbageCollected<DocumentMarkerVector>();
  auto* spelling = MakeGarbageCollected<DocumentMarkerVector>();
  auto* target = MakeGarbageCollected<DocumentMarkerVector>();
  Highlight* foo_highlight = Highlight::Create(
      *MakeGarbageCollected<HeapVector<Member<AbstractRange>>>());
  Highlight* bar_highlight = Highlight::Create(
      *MakeGarbageCollected<HeapVector<Member<AbstractRange>>>());
  registry->SetForTesting("foo", foo_highlight);
  registry->SetForTesting("bar", bar_highlight);

  HighlightLayer orig{HighlightLayerType::kOriginating};
  HighlightLayer foo{HighlightLayerType::kCustom, "foo"};
  HighlightLayer bar{HighlightLayerType::kCustom, "bar"};
  HighlightLayer spel{HighlightLayerType::kSpelling};
  HighlightLayer targ{HighlightLayerType::kTargetText};
  HighlightLayer sele{HighlightLayerType::kSelection};
  NGTextFragmentPaintInfo originating{"", 0, 25};
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(0, 14, "foo", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(10, 19, "bar", nullptr));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(6, 9, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(10, 14, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(20, 23, ""));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(15, 23));
  LayoutSelectionStatus selection{13, 19, SelectSoftLineBreak::kNotSelected};

  Vector<HighlightLayer> layers = NGHighlightOverlay::ComputeLayers(
      registry, &selection, *custom, *grammar, *spelling, *target);

  // 0     6   10   15   20  24
  // brown fxo oevr lazy dgo today
  // [                       ]        originating
  //                                  ::highlight(foo), not active
  //                                  ::highlight(bar), not active
  //                                  ::spelling-error, not active
  //                                  ::target-text, not active
  //                                  ::selection, not active

  Vector<HighlightEdge> edges = NGHighlightOverlay::ComputeEdges(
      node, registry, false, originating, nullptr, *none, *none, *none, *none);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(originating, layers, edges),
            (Vector<HighlightPart>{
                HighlightPart{orig, 0, 25, {orig}},
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

  Vector<HighlightEdge> edges2 = NGHighlightOverlay::ComputeEdges(
      node, registry, false, originating, &selection, *custom, *grammar,
      *spelling, *target);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(originating, layers, edges2),
            (Vector<HighlightPart>{
                HighlightPart{foo, 0, 6, {orig, foo}},
                HighlightPart{spel, 6, 9, {orig, foo, spel}},
                HighlightPart{foo, 9, 10, {orig, foo}},
                HighlightPart{spel, 10, 13, {orig, foo, bar, spel}},
                HighlightPart{sele, 13, 14, {orig, foo, bar, spel, sele}},
                HighlightPart{sele, 14, 15, {orig, bar, sele}},
                HighlightPart{sele, 15, 19, {orig, bar, targ, sele}},
                HighlightPart{targ, 19, 20, {orig, targ}},
                HighlightPart{targ, 20, 23, {orig, spel, targ}},
                HighlightPart{orig, 23, 25, {orig}},
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
  Vector<HighlightEdge> edges3 = NGHighlightOverlay::ComputeEdges(
      node, registry, false, originating, &selection, *custom, *grammar,
      *spelling, *target);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(originating, layers, edges3),
            (Vector<HighlightPart>{
                HighlightPart{orig, 0, 6, {orig}},
                HighlightPart{spel, 6, 9, {orig, foo, spel}},
                HighlightPart{foo, 9, 10, {orig, foo}},
                HighlightPart{spel, 10, 13, {orig, foo, bar, spel}},
                HighlightPart{sele, 13, 14, {orig, foo, bar, spel, sele}},
                HighlightPart{sele, 14, 15, {orig, bar, sele}},
                HighlightPart{sele, 15, 19, {orig, bar, targ, sele}},
                HighlightPart{targ, 19, 20, {orig, targ}},
                HighlightPart{targ, 20, 23, {orig, spel, targ}},
                HighlightPart{orig, 23, 25, {orig}},
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

  NGTextFragmentPaintInfo originating2{"", 8, 18};
  Vector<HighlightEdge> edges4 = NGHighlightOverlay::ComputeEdges(
      node, registry, false, originating, &selection, *custom, *grammar,
      *spelling, *target);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(originating2, layers, edges4),
            (Vector<HighlightPart>{
                HighlightPart{spel, 8, 9, {orig, foo, spel}},
                HighlightPart{foo, 9, 10, {orig, foo}},
                HighlightPart{spel, 10, 13, {orig, foo, bar, spel}},
                HighlightPart{sele, 13, 14, {orig, foo, bar, spel, sele}},
                HighlightPart{sele, 14, 15, {orig, bar, sele}},
                HighlightPart{sele, 15, 18, {orig, bar, targ, sele}},
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

  Vector<HighlightEdge> edges5 =
      NGHighlightOverlay::ComputeEdges(node, registry, false, originating,
                                       nullptr, *none, *none, *spelling, *none);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(originating2, layers, edges5),
            (Vector<HighlightPart>{
                HighlightPart{spel, 8, 9, {orig, spel}},
                HighlightPart{orig, 9, 10, {orig}},
                HighlightPart{spel, 10, 14, {orig, spel}},
                HighlightPart{orig, 14, 18, {orig}},
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

  NGTextFragmentPaintInfo originating3{"", 1, 4};
  Vector<HighlightEdge> edges6 = NGHighlightOverlay::ComputeEdges(
      node, registry, false, originating, &selection, *custom, *grammar,
      *spelling, *target);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(originating3, layers, edges6),
            (Vector<HighlightPart>{
                HighlightPart{orig, 1, 4, {orig}},
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

  NGTextFragmentPaintInfo originating4{"", 25, 28};
  Vector<HighlightEdge> edges7 = NGHighlightOverlay::ComputeEdges(
      node, registry, false, originating, &selection, *custom, *grammar,
      *spelling, *target);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(originating4, layers, edges7),
            (Vector<HighlightPart>{
                HighlightPart{orig, 25, 28, {orig}},
            }))
      << "correct when last edge ends before start of originating fragment";
}

}  // namespace blink
