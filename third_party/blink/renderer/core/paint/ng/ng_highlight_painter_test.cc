// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_highlight_painter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_text_painter.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class NGHighlightPainterTest : public PaintControllerPaintTest,
                               private ScopedHighlightOverlayPaintingForTest,
                               private ScopedLayoutNGForTest {
 public:
  explicit NGHighlightPainterTest(
      LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client),
        ScopedHighlightOverlayPaintingForTest(true),
        ScopedLayoutNGForTest(true) {}
};

INSTANTIATE_PAINT_TEST_SUITE_P(NGHighlightPainterTest);

TEST_P(NGHighlightPainterTest, FastSpellingGrammarPaintCase) {
  auto test = [&](String stylesheet) {
    SetBodyInnerHTML("x<style>" + stylesheet + "</style>");
    UpdateAllLifecyclePhasesForTest();
    const Node& text = *GetDocument().body()->firstChild();
    return EphemeralRange{Position{text, 0}, Position{text, 1}};
  };

  auto expect = [&](NGHighlightPainter::Case expected, unsigned line) {
    LayoutObject& body = *GetDocument().body()->GetLayoutObject();
    const auto& block_flow = To<LayoutNGBlockFlow>(body);
    NGInlinePaintContext inline_context{};
    NGInlineCursor cursor{block_flow};
    cursor.MoveToFirstLine();
    inline_context.SetLineBox(cursor);
    cursor.MoveTo(*block_flow.FirstChild());

    CullRect cull_rect{};
    gfx::Rect rect{};
    PhysicalOffset physical_offset{};
    PhysicalRect physical_rect{};
    const NGFragmentItem& text_item = *cursor.CurrentItem();
    const ComputedStyle& style = text_item.Style();
    absl::optional<NGHighlightPainter::SelectionPaintState> maybe_selection;
    NGHighlightPainter::SelectionPaintState* selection = nullptr;
    if (text_item.GetLayoutObject()->IsSelected()) {
      maybe_selection.emplace(cursor, physical_offset);
      if (maybe_selection->Status().HasValidRange())
        selection = &*maybe_selection;
    }

    GraphicsContext graphics_context{RootPaintController()};
    PaintInfo paint_info{graphics_context, cull_rect, PaintPhase::kForeground};
    TextPaintStyle text_style =
        TextPainterBase::TextPaintingStyle(GetDocument(), style, paint_info);
    NGTextPainter text_painter(graphics_context, text_item.ScaledFont(), rect,
                               physical_offset, physical_rect, &inline_context,
                               true);
    NGTextDecorationPainter decoration_painter(text_painter, text_item,
                                               paint_info, style, text_style,
                                               physical_rect, selection);
    NGHighlightPainter highlight_painter(
        cursor.Current()->TextPaintInfo(cursor.Items()), text_painter,
        decoration_painter, paint_info, cursor, text_item, {}, physical_rect,
        physical_offset, style, text_style, selection, false);

    EXPECT_EQ(highlight_painter.PaintCase(), expected)
        << "(line " << line << ")";
  };

  // kFastSpellingGrammar only if there are spelling and/or grammar highlights.
  test("");
  expect(NGHighlightPainter::kNoHighlights, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(""));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddGrammarMarker(test(""));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // Overlapping spelling and grammar highlights is ok.
  EphemeralRange range = test("");
  GetDocument().Markers().AddSpellingMarker(range);
  GetDocument().Markers().AddGrammarMarker(range);
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // Overlapping selection highlight is not ok.
  Selection().SelectAll();
  range = test("");
  GetDocument().Markers().AddSpellingMarker(range);
  GetDocument().Markers().AddGrammarMarker(range);
  expect(NGHighlightPainter::kOverlay, __LINE__);
  Selection().Clear();

  // Non-trivial spelling style is still ok if there are no spelling highlights.
  range = test("::spelling-error { background-color: green; }");
  GetDocument().Markers().AddGrammarMarker(range);
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // Non-trivial grammar style is still ok if there are no grammar highlights.
  range = test("::grammar-error { background-color: green; }");
  GetDocument().Markers().AddSpellingMarker(range);
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘color’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: green; }
      ::spelling-error { color: red; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: green; }
      ::spelling-error { color: green; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘-webkit-text-fill-color’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; -webkit-text-fill-color: green; }
      ::spelling-error { /* -webkit-text-fill-color = blue */ }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; -webkit-text-fill-color: green; }
      ::spelling-error { -webkit-text-fill-color: red; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; -webkit-text-fill-color: green; }
      ::spelling-error { -webkit-text-fill-color: green; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘-webkit-text-stroke-color’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; -webkit-text-stroke-color: green; }
      ::spelling-error { /* -webkit-text-stroke-color = blue */ }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; -webkit-text-stroke-color: green; }
      ::spelling-error { -webkit-text-stroke-color: red; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; -webkit-text-stroke-color: green; }
      ::spelling-error { -webkit-text-stroke-color: green; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘-webkit-text-stroke-width’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { -webkit-text-stroke-width: 1px; }
      ::spelling-error { /* -webkit-text-stroke-width = 0 */ }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { -webkit-text-stroke-width: 1px; }
      ::spelling-error { -webkit-text-stroke-width: 2px; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { -webkit-text-stroke-width: 1px; }
      ::spelling-error { -webkit-text-stroke-width: 1px; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘background-color’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { background-color: red; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: red; }
      ::spelling-error { background-color: currentColor; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { background-color: #66339900; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: #66339900; }
      ::spelling-error { background-color: currentColor; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘text-shadow’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { text-shadow: 0 0 currentColor; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);

  // ‘text-decoration’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { text-decoration: none; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { text-decoration: grammar-error; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddGrammarMarker(test(R"HTML(
      ::grammar-error { text-decoration: spelling-error; }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { text-decoration: spelling-error; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddGrammarMarker(test(R"HTML(
      ::grammar-error { text-decoration: grammar-error; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);

  // originating ‘text-decoration’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; text-decoration: underline; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      html { color: blue; text-decoration: underline; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: red; text-decoration: blue underline; }
      ::spelling-error { /* decoration recolored to red */ }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      html { color: red; text-decoration: blue underline; }
      ::spelling-error { /* decoration recolored to red */ }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);

  // ‘text-emphasis-color’
  // TODO(crbug.com/1147859) clean up when spec issue is resolved again
  // https://github.com/w3c/csswg-drafts/issues/7101
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; text-emphasis: circle; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; }
      ::spelling-error { /* no emphasis */ text-emphasis-color: green; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: red; text-emphasis: blue circle; }
      ::spelling-error { /* emphasis recolored to red */ }
  )HTML"));
  expect(NGHighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: red; text-emphasis: blue circle; }
      ::spelling-error { text-emphasis-color: blue; }
  )HTML"));
  expect(NGHighlightPainter::kFastSpellingGrammar, __LINE__);
}

}  // namespace blink
