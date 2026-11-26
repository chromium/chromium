// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/highlight_painter.h"

#include <initializer_list>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/editing_state.h"
#include "third_party/blink/renderer/core/editing/commands/split_text_node_command.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/ime/ime_text_span.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/line_relative_rect.h"
#include "third_party/blink/renderer/core/paint/marker_range_mapping_context.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/text_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/text_paint_style.h"
#include "third_party/blink/renderer/core/paint/text_painter.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/ime/mojom/ime_types.mojom-blink.h"

namespace blink {

class HighlightPainterTest : public PaintControllerPaintTest {
 public:
  explicit HighlightPainterTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client) {}

 protected:
  void ExpectCompositionBackground(std::initializer_list<gfx::Rect> rects,
                                   std::initializer_list<int> sample_rows) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(80, 40);
    bitmap.eraseColor(SK_ColorWHITE);
    SkCanvas canvas(bitmap);
    GetDocument().View()->GetPaintRecord().Playback(&canvas);
    for (int y : sample_rows) {
      for (int x = 0; x < bitmap.width(); ++x) {
        bool highlighted = false;
        for (const auto& rect : rects) {
          highlighted |= rect.Contains(x, y);
        }
        EXPECT_EQ(highlighted ? SK_ColorRED : SK_ColorWHITE,
                  bitmap.getColor(x, y))
            << "at " << x << ", " << y;
      }
    }
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(HighlightPainterTest);

TEST_P(HighlightPainterTest, TextMatchMarkersAfterRedoAndShrink) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      #editor {
        font: 24px Ahem;
        text-transform: uppercase;
        inline-size: 9px;
        word-break: break-all;
        overflow-wrap: anywhere;
      }
    </style>
    <div id="editor" contenteditable spellcheck="false">&#xdf;x &#xdf;x</div>
    <div id="seed"></div>
  )HTML");

  auto* editor = GetElementById("editor");
  auto* source = To<Text>(editor->firstChild());
  const String match(u"\u00DFx");
  SimpleEditCommand* command =
      MakeGarbageCollected<SplitTextNodeCommand>(source, 4);
  EditingState editing_state;
  command->DoApply(&editing_state);
  ASSERT_FALSE(editing_state.IsAborted());
  auto* prefix = To<Text>(editor->firstChild());
  ASSERT_NE(source, prefix);
  command->DoUnapply();
  ASSERT_EQ(source, editor->firstChild());

  // Redo reuses the prefix and the find markers it acquired in seed.
  prefix->setData("aaa" + match);
  GetElementById("seed")->AppendChild(prefix);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  auto& controller = GetDocument().Markers();
  for (unsigned start : {0u, 3u}) {
    controller.AddTextMatchMarker(
        EphemeralRange(Position(source, start), Position(source, start + 2)),
        TextMatchMarker::MatchStatus::kInactive);
  }
  controller.AddTextMatchMarker(
      EphemeralRange(Position(prefix, 3), Position(prefix, 5)),
      TextMatchMarker::MatchStatus::kInactive);

  command->DoReapply();
  ASSERT_EQ(prefix, editor->firstChild());
  prefix->deleteData(0, 3, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(match, prefix->data());

  const auto markers =
      controller.MarkersFor(*prefix, DocumentMarker::MarkerTypes::TextMatch());
  ASSERT_EQ(2u, markers.size());
  EXPECT_EQ(0u, markers[0]->StartOffset());
  EXPECT_EQ(0u, markers[0]->EndOffset());
  EXPECT_EQ(0u, markers[1]->StartOffset());
  EXPECT_EQ(2u, markers[1]->EndOffset());

  UpdateAllLifecyclePhasesExceptPaint();
  InlineCursor cursor;
  cursor.MoveTo(*prefix->GetLayoutObject());
  ASSERT_TRUE(cursor);
  ASSERT_TRUE(cursor.Current().IsText());
  ASSERT_EQ(1u, cursor.Current().TextEndOffset() -
                    cursor.Current().TextStartOffset());
  // The first S of uppercased sharp s paints a collapsed DOM range.
  const auto offsets = HighlightPainter::GetFragmentDOMOffsets(
      *prefix, cursor.Current().TextStartOffset(),
      cursor.Current().TextEndOffset());
  EXPECT_EQ(0u, offsets.start);
  EXPECT_EQ(0u, offsets.end);

  UpdateAllLifecyclePhasesForTest();
}

TEST_P(HighlightPainterTest, FastSpellingGrammarPaintCase) {
  auto test = [&](String stylesheet) {
    SetBodyInnerHTML("x<style>" + stylesheet + "</style>");
    UpdateAllLifecyclePhasesForTest();
    const Node& text = *GetDocument().body()->firstChild();
    return EphemeralRange{Position{text, 0}, Position{text, 1}};
  };

  auto expect = [&](HighlightPainter::Case expected, unsigned line) {
    LayoutObject& body = *GetDocument().body()->GetLayoutObject();
    const auto& block_flow = To<LayoutBlockFlow>(body);
    InlinePaintContext inline_context{};
    InlineCursor cursor{block_flow};
    cursor.MoveToFirstLine();
    inline_context.SetLineBox(cursor);
    cursor.MoveTo(*block_flow.FirstChild());

    CullRect cull_rect{};
    gfx::Rect rect{};
    PhysicalOffset physical_offset{};
    PhysicalRect physical_rect{};
    const FragmentItem& text_item = *cursor.CurrentItem();
    const ComputedStyle& style = text_item.Style();
    std::optional<HighlightPainter::SelectionPaintState> maybe_selection;
    HighlightPainter::SelectionPaintState* selection = nullptr;
    if (text_item.GetLayoutObject()->IsSelected()) {
      maybe_selection.emplace(cursor, physical_offset);
      if (maybe_selection->Status().HasValidRange())
        selection = &*maybe_selection;
    }

    PaintController controller;
    GraphicsContext graphics_context(controller);
    PaintInfo paint_info(graphics_context, cull_rect, PaintPhase::kForeground,
                         /*descendant_painting_blocked=*/false);
    TextPaintStyle text_style =
        TextPainter::TextPaintingStyle(GetDocument(), style, paint_info);
    if (selection) {
      selection->ComputeSelectionStyle(GetDocument(), style,
                                       text_item.GetLayoutObject()->GetNode(),
                                       paint_info, text_style);
    }
    LineRelativeRect rotated_rect =
        LineRelativeRect::CreateFromLineBox(physical_rect, true);
    TextPainter text_painter(
        graphics_context, /*svg_context_paints*/ nullptr,
        text_item.ScaledFont(), rect,
        LineRelativeOffset::CreateFromBoxOrigin(physical_offset));
    TextDecorationPainter decoration_painter(
        text_painter, &inline_context, paint_info, style, text_style,
        rotated_rect, selection, TextDecorationFragmentContext());
    HighlightPainter highlight_painter(
        cursor.Current()->TextPaintInfo(cursor.Items()), text_painter,
        decoration_painter, paint_info, cursor, text_item, physical_offset,
        style, text_style, selection);

    EXPECT_EQ(highlight_painter.PaintCase(), expected)
        << "(line " << line << ")";
  };

  // kFastSpellingGrammar only if there are spelling and/or grammar highlights.
  test("");
  expect(HighlightPainter::kNoHighlights, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(""));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddGrammarMarker(test(""));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);

  // Overlapping spelling and grammar highlights is ok.
  EphemeralRange range = test("");
  GetDocument().Markers().AddSpellingMarker(range);
  GetDocument().Markers().AddGrammarMarker(range);
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);

  // Overlapping selection highlight is not ok.
  Selection().SelectAll();
  range = test("");
  GetDocument().Markers().AddSpellingMarker(range);
  GetDocument().Markers().AddGrammarMarker(range);
  expect(HighlightPainter::kOverlay, __LINE__);
  Selection().Clear();

  // Non-trivial spelling style is still ok if there are no spelling highlights.
  range = test("::spelling-error { background-color: green; }");
  GetDocument().Markers().AddGrammarMarker(range);
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);

  // Non-trivial grammar style is still ok if there are no grammar highlights.
  range = test("::grammar-error { background-color: green; }");
  GetDocument().Markers().AddSpellingMarker(range);
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘color’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: green; }
      ::spelling-error { color: red; }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: green; }
      ::spelling-error { color: green; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘background-color’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { background-color: red; }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: red; }
      ::spelling-error { background-color: currentColor; }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { background-color: #66339900; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: #66339900; }
      ::spelling-error { background-color: currentColor; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);

  // ‘text-shadow’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { text-shadow: 0 0 currentColor; }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);

  // ‘text-decoration’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { text-decoration: none; }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { text-decoration: grammar-error; }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddGrammarMarker(test(R"HTML(
      ::grammar-error { text-decoration: spelling-error; }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      ::spelling-error { text-decoration: spelling-error; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddGrammarMarker(test(R"HTML(
      ::grammar-error { text-decoration: grammar-error; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);

  // originating ‘text-decoration’
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; text-decoration: underline; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      html { color: blue; text-decoration: underline; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: red; text-decoration: blue underline; }
      ::spelling-error { /* decoration recolored to red */ }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      html { color: red; text-decoration: blue underline; }
      ::spelling-error { /* decoration recolored to red */ }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);

  // ‘text-emphasis-color’
  // TODO(crbug.com/1147859) clean up when spec issue is resolved again
  // https://github.com/w3c/csswg-drafts/issues/7101
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; text-emphasis: circle; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: blue; }
      ::spelling-error { /* no emphasis */ text-emphasis-color: green; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: red; text-emphasis: blue circle; }
      ::spelling-error { /* emphasis recolored to red */ }
  )HTML"));
  expect(HighlightPainter::kOverlay, __LINE__);
  GetDocument().Markers().AddSpellingMarker(test(R"HTML(
      body { color: red; text-emphasis: blue circle; }
      ::spelling-error { text-emphasis-color: blue; }
  )HTML"));
  expect(HighlightPainter::kFastSpellingGrammar, __LINE__);
}

TEST_P(HighlightPainterTest, CompositionAfterPreservedLeadingSpaces) {
  LoadAhem();
  for (const char* prefix : {" ", "  ", "   "}) {
    const String spaces(prefix);
    SCOPED_TRACE(spaces.length());
    SetBodyInnerHTML(
        "<style>body{margin:0}"
        "#sample{white-space:pre-wrap;font:10px/10px Ahem;"
        "color:transparent;caret-color:transparent}</style>"
        "<div id='sample' contenteditable>" +
        spaces + "</div>");
    UpdateAllLifecyclePhasesForTest();
    auto* sample = GetElementById("sample");
    sample->Focus();
    auto& controller = GetFrame().GetInputMethodController();
    ASSERT_TRUE(controller.SetEditableSelectionOffsets(
        PlainTextRange(spaces.length(), spaces.length())));
    const Vector<ImeTextSpan> spans = {
        ImeTextSpan(ImeTextSpan::Type::kComposition, 0, 2, Color::kTransparent,
                    ui::mojom::ImeTextSpanThickness::kNone,
                    ui::mojom::ImeTextSpanUnderlineStyle::kNone,
                    Color::kTransparent, Color(255, 0, 0))};
    controller.SetComposition("ni", spans, 2, 2);
    ASSERT_TRUE(controller.HasComposition());

    // A real preedit must survive painting before the committed text arrives.
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(spaces + "ni", sample->textContent());
    const auto markers = GetDocument().Markers().Markers();
    ASSERT_EQ(1u, markers.size());
    EXPECT_EQ(DocumentMarker::kComposition, markers[0]->GetType());
    EXPECT_EQ(spaces.length(), markers[0]->StartOffset());
    EXPECT_EQ(spaces.length() + 2, markers[0]->EndOffset());
    // Sample the text line and the empty line below it.
    ExpectCompositionBackground({gfx::Rect(10 * spaces.length(), 0, 20, 10)},
                                {5, 15});

    ASSERT_TRUE(controller.CommitText(String::FromUtf8("\xE4\xBD\xA0"), {}, 0));
    UpdateAllLifecyclePhasesForTest();
    EXPECT_FALSE(controller.HasComposition());
    EXPECT_EQ(spaces + String::FromUtf8("\xE4\xBD\xA0"), sample->textContent());
    ExpectCompositionBackground({}, {5, 15});
  }
}

TEST_P(HighlightPainterTest, CompositionBackgroundAcrossLineFragments) {
  LoadAhem();
  SetBodyInnerHTML(
      "<style>body{margin:0}"
      "#sample{width:20px;white-space:pre-wrap;word-break:break-all;"
      "font:10px/10px Ahem;color:transparent;caret-color:transparent}</style>"
      "<div id='sample' contenteditable> </div>");
  UpdateAllLifecyclePhasesForTest();
  GetElementById("sample")->Focus();
  auto& controller = GetFrame().GetInputMethodController();
  ASSERT_TRUE(controller.SetEditableSelectionOffsets(PlainTextRange(1, 1)));
  const Vector<ImeTextSpan> spans = {
      ImeTextSpan(ImeTextSpan::Type::kComposition, 0, 5, Color::kTransparent,
                  ui::mojom::ImeTextSpanThickness::kNone,
                  ui::mojom::ImeTextSpanUnderlineStyle::kNone,
                  Color::kTransparent, Color(255, 0, 0))};
  controller.SetComposition("nihao", spans, 5, 5);
  ASSERT_TRUE(controller.HasComposition());
  UpdateAllLifecyclePhasesForTest();

  // The first fragment ends inside the composition; the remaining characters
  // wrap onto two more lines. The leading space must remain unhighlighted.
  // Sample all three text lines and the empty line below them.
  ExpectCompositionBackground(
      {gfx::Rect(10, 0, 10, 10), gfx::Rect(0, 10, 20, 20)}, {5, 15, 25, 35});
}

TEST_P(HighlightPainterTest, CompositionMarkerInCollapsedWhitespace) {
  LoadAhem();
  SetBodyInnerHTML(
      "<style>body{margin:0}"
      "#sample{font:10px/10px Ahem;color:transparent}</style>"
      "<style id='preserve'>#sample{white-space:pre}</style>"
      "<div id='sample'>a  b</div>");
  UpdateAllLifecyclePhasesForTest();
  auto* text = To<Text>(GetElementById("sample")->firstChild());
  auto& controller = GetDocument().Markers();
  controller.AddCompositionMarker(
      EphemeralRange(Position(text, 2), Position(text, 3)), Color::kTransparent,
      ui::mojom::ImeTextSpanThickness::kNone,
      ui::mojom::ImeTextSpanUnderlineStyle::kNone, Color::kTransparent,
      Color(255, 0, 0));
  const auto markers = controller.Markers();
  ASSERT_EQ(1u, markers.size());
  // Keep the DOM marker while changing its whitespace from visible to
  // collapsed, so the painter receives an empty mapped text range.
  GetElementById("preserve")->setTextContent("#sample{white-space:normal}");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(1u, controller.Markers().size());
  MarkerRangeMappingContext mapping_context(*text, *text->GetLayoutObject(),
                                            {0, text->length()});
  const auto offsets = mapping_context.GetTextContentOffsets(*markers[0]);
  ASSERT_TRUE(offsets);
  ASSERT_EQ(offsets->start, offsets->end);

  ExpectCompositionBackground({}, {5, 15});
}

}  // namespace blink
