// Copyright 2022 The Chromium Authors. All rights reserved.
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
                               private ScopedHighlightOverlayPaintingForTest,
                               private ScopedLayoutNGForTest {
 public:
  NGHighlightOverlayTest()
      : ScopedHighlightOverlayPaintingForTest(true),
        ScopedLayoutNGForTest(true) {}
};

TEST_F(NGHighlightOverlayTest, ComputeLayers) {
  auto* registry =
      MakeGarbageCollected<HighlightRegistry>(*GetFrame().DomWindow());
  NGTextFragmentPaintInfo originating{};
  LayoutSelectionStatus selection{0, 0, SelectSoftLineBreak::kNotSelected};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();

  EXPECT_EQ(
      NGHighlightOverlay::ComputeLayers(registry, originating, nullptr, *none,
                                        *none, *none, *none),
      Vector<HighlightLayer>{HighlightLayer{HighlightLayerType::kOriginating}})
      << "should return kOriginating when nothing is highlighted";

  EXPECT_EQ(NGHighlightOverlay::ComputeLayers(registry, originating, &selection,
                                              *none, *none, *none, *none),
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

  EXPECT_EQ(
      NGHighlightOverlay::ComputeLayers(registry, originating, nullptr, *none,
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

  EXPECT_EQ(NGHighlightOverlay::ComputeLayers(registry, originating, nullptr,
                                              *custom, *none, *none, *none),
            (Vector<HighlightLayer>{
                HighlightLayer{HighlightLayerType::kOriginating},
                HighlightLayer{HighlightLayerType::kCustom, "foo"},
                HighlightLayer{HighlightLayerType::kCustom, "bar"},
            }))
      << "should return kCustom layers no more than once each";
}

TEST_F(NGHighlightOverlayTest, ComputeEdges) {
  SetBodyInnerHTML(R"HTML(   foo<br>)HTML");
  const Node* node = GetDocument().body()->childNodes()->item(0);
  const Node* br = GetDocument().body()->childNodes()->item(1);
  UpdateAllLifecyclePhasesForTest();

  auto* registry =
      MakeGarbageCollected<HighlightRegistry>(*GetFrame().DomWindow());
  NGTextFragmentPaintInfo originating{"", 1, 3};
  LayoutSelectionStatus selection{0, 2, SelectSoftLineBreak::kNotSelected};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();
  HighlightLayer originating_layer{HighlightLayerType::kOriginating};
  HighlightLayer selection_layer{HighlightLayerType::kSelection};
  HighlightLayer grammar_layer{HighlightLayerType::kGrammar};
  HighlightLayer spelling_layer{HighlightLayerType::kSpelling};
  HighlightLayer target_layer{HighlightLayerType::kTargetText};

  EXPECT_EQ(
      NGHighlightOverlay::ComputeEdges(node, registry, originating, nullptr,
                                       *none, *none, *none, *none),
      (Vector<HighlightEdge>{
          HighlightEdge{1, originating_layer, HighlightEdgeType::kStart},
          HighlightEdge{3, originating_layer, HighlightEdgeType::kEnd},
      }))
      << "should return originating edges when nothing is highlighted";

  EXPECT_EQ(
      NGHighlightOverlay::ComputeEdges(nullptr, registry, originating,
                                       &selection, *none, *none, *none, *none),
      (Vector<HighlightEdge>{
          HighlightEdge{0, selection_layer, HighlightEdgeType::kStart},
          HighlightEdge{1, originating_layer, HighlightEdgeType::kStart},
          HighlightEdge{2, selection_layer, HighlightEdgeType::kEnd},
          HighlightEdge{3, originating_layer, HighlightEdgeType::kEnd},
      }))
      << "should still return non-marker edges when node is nullptr";

  EXPECT_EQ(
      NGHighlightOverlay::ComputeEdges(br, registry, originating, &selection,
                                       *none, *none, *none, *none),
      (Vector<HighlightEdge>{
          HighlightEdge{0, selection_layer, HighlightEdgeType::kStart},
          HighlightEdge{1, originating_layer, HighlightEdgeType::kStart},
          HighlightEdge{2, selection_layer, HighlightEdgeType::kEnd},
          HighlightEdge{3, originating_layer, HighlightEdgeType::kEnd},
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

  EXPECT_EQ(
      NGHighlightOverlay::ComputeEdges(node, registry, originating, &selection,
                                       *none, *grammar, *spelling, *target),
      (Vector<HighlightEdge>{
          HighlightEdge{0, grammar_layer, HighlightEdgeType::kStart},
          HighlightEdge{0, selection_layer, HighlightEdgeType::kStart},
          HighlightEdge{1, grammar_layer, HighlightEdgeType::kEnd},
          HighlightEdge{1, originating_layer, HighlightEdgeType::kStart},
          HighlightEdge{1, grammar_layer, HighlightEdgeType::kStart},
          HighlightEdge{1, spelling_layer, HighlightEdgeType::kStart},
          HighlightEdge{1, target_layer, HighlightEdgeType::kStart},
          HighlightEdge{2, grammar_layer, HighlightEdgeType::kEnd},
          HighlightEdge{2, spelling_layer, HighlightEdgeType::kEnd},
          HighlightEdge{2, target_layer, HighlightEdgeType::kEnd},
          HighlightEdge{2, selection_layer, HighlightEdgeType::kEnd},
          HighlightEdge{2, spelling_layer, HighlightEdgeType::kStart},
          HighlightEdge{3, originating_layer, HighlightEdgeType::kEnd},
          HighlightEdge{3, spelling_layer, HighlightEdgeType::kEnd},
      }))
      << "should return edges in correct order";
}

TEST_F(NGHighlightOverlayTest, ComputeParts) {
  SetBodyInnerHTML(R"HTML(brown fxo oevr lazy dgo)HTML");
  const Node* node = GetDocument().body()->childNodes()->item(0);
  UpdateAllLifecyclePhasesForTest();

  auto* registry =
      MakeGarbageCollected<HighlightRegistry>(*GetFrame().DomWindow());
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

  // 0     6   10   15   20
  // brown fxo oevr lazy dgo
  // [                     ]  originating
  //       [      ]           ::highlight(foo)
  //           [       ]      ::highlight(bar)
  //       [ ] [  ]      [ ]  ::spelling-error
  //                [      ]  ::target-text
  //              [    ]      ::selection

  HighlightLayer orig{HighlightLayerType::kOriginating};
  HighlightLayer foo{HighlightLayerType::kCustom, "foo"};
  HighlightLayer bar{HighlightLayerType::kCustom, "bar"};
  HighlightLayer spel{HighlightLayerType::kSpelling};
  HighlightLayer targ{HighlightLayerType::kTargetText};
  HighlightLayer sele{HighlightLayerType::kSelection};
  NGTextFragmentPaintInfo originating{"", 0, 23};
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(6, 14, "foo", nullptr));
  custom->push_back(
      MakeGarbageCollected<CustomHighlightMarker>(10, 19, "bar", nullptr));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(6, 9, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(10, 14, ""));
  spelling->push_back(MakeGarbageCollected<SpellingMarker>(20, 23, ""));
  target->push_back(MakeGarbageCollected<TextFragmentMarker>(15, 23));
  LayoutSelectionStatus selection{13, 19, SelectSoftLineBreak::kNotSelected};

  Vector<HighlightLayer> layers = NGHighlightOverlay::ComputeLayers(
      registry, originating, &selection, *custom, *grammar, *spelling, *target);
  Vector<HighlightEdge> edges =
      NGHighlightOverlay::ComputeEdges(node, registry, originating, &selection,
                                       *custom, *grammar, *spelling, *target);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(layers, edges),
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
            }))
      << "should return correct parts";

  // 0     6   10   15   20
  // brown fxo oevr lazy dgo
  //                      []  originating, changed!
  //       [      ]           ::highlight(foo), as above
  //           [       ]      ::highlight(bar), as above
  //       [ ] [  ]      [ ]  ::spelling-error, as above
  //                [      ]  ::target-text, as above
  //              [    ]      ::selection, as above

  NGTextFragmentPaintInfo originating2{"", 21, 23};
  Vector<HighlightEdge> edges2 =
      NGHighlightOverlay::ComputeEdges(node, registry, originating2, &selection,
                                       *custom, *grammar, *spelling, *target);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(layers, edges2),
            (Vector<HighlightPart>{
                HighlightPart{spel, 6, 9, {foo, spel}},
                HighlightPart{foo, 9, 10, {foo}},
                HighlightPart{spel, 10, 13, {foo, bar, spel}},
                HighlightPart{sele, 13, 14, {foo, bar, spel, sele}},
                HighlightPart{sele, 14, 15, {bar, sele}},
                HighlightPart{sele, 15, 19, {bar, targ, sele}},
                HighlightPart{targ, 19, 20, {targ}},
                HighlightPart{targ, 20, 21, {spel, targ}},
                HighlightPart{targ, 21, 23, {orig, spel, targ}},
            }))
      << "should not crash if the first edge has offset > 0";

  // 0     6   10   15   20
  // brown fxo oevr lazy dgo
  // [       ]                originating, changed!
  //              [    ]      ::selection, as above
  //          ^^^^            gap where no layers are active!

  NGTextFragmentPaintInfo originating3{"", 0, 9};
  auto* none = MakeGarbageCollected<DocumentMarkerVector>();
  Vector<HighlightEdge> edges3 = NGHighlightOverlay::ComputeEdges(
      node, registry, originating3, &selection, *none, *none, *none, *none);

  EXPECT_EQ(NGHighlightOverlay::ComputeParts(layers, edges3),
            (Vector<HighlightPart>{
                HighlightPart{orig, 0, 9, {orig}},
                HighlightPart{sele, 13, 19, {sele}},
            }))
      << "should not crash if there is a gap in active layers";
}

}  // namespace blink
