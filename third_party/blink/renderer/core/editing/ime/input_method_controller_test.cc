// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

using ui::mojom::ImeTextSpanThickness;
using ui::mojom::ImeTextSpanUnderlineStyle;

namespace blink {

class InputMethodControllerTest : public EditingTestBase {
 protected:
  enum SelectionType { kNoSelection, kCaretSelection, kRangeSelection };

  InputMethodController& Controller() {
    return GetFrame().GetInputMethodController();
  }

  // TODO(editing-dev): We should use |CompositionEphemeralRange()| instead
  // of having |GetCompositionRange()| and marking |InputMethodControllerTest|
  // as friend class.
  Range* GetCompositionRange() { return Controller().composition_range_.Get(); }

  Element* InsertHTMLElement(const char* element_code, const char* element_id);
  void CreateHTMLWithCompositionInputEventListeners();
  void CreateHTMLWithCompositionEndEventListener(const SelectionType);
};

Element* InputMethodControllerTest::InsertHTMLElement(const char* element_code,
                                                      const char* element_id) {
  GetDocument().write(element_code);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Element* element = GetElementById(element_id);
  element->Focus();
  return element;
}

void InputMethodControllerTest::CreateHTMLWithCompositionInputEventListeners() {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* editable =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('beforeinput', "
      "event => {"
      "  document.title = `beforeinput.data:${event.data};`;"
      "  document.title += 'beforeinput.targetRanges:';"
      "  const range = event.getTargetRanges()[0];"
      "  if (range !== undefined) {"
      "    document.title += `${range.startOffset}-${range.endOffset};`;"
      "  } else document.title += ';';"
      "});"
      "document.getElementById('sample').addEventListener('input', "
      "  event => document.title += `input.data:${event.data};`);"
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => document.title += `compositionend.data:${event.data};`);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();
  editable->Focus();
}

void InputMethodControllerTest::CreateHTMLWithCompositionEndEventListener(
    const SelectionType type) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* editable =
      InsertHTMLElement("<div id='sample' contentEditable></div>", "sample");
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);

  switch (type) {
    case kNoSelection:
      script->setInnerHTML(
          // If the caret position is set before firing 'compositonend' event
          // (and it should), the final caret position will be reset to null.
          "document.getElementById('sample').addEventListener('compositionend',"
          "  event => getSelection().removeAllRanges());");
      break;
    case kCaretSelection:
      script->setInnerHTML(
          // If the caret position is set before firing 'compositonend' event
          // (and it should), the final caret position will be reset to [3,3].
          "document.getElementById('sample').addEventListener('compositionend',"
          "  event => {"
          "    const node = document.getElementById('sample').firstChild;"
          "    getSelection().collapse(node, 3);"
          "});");
      break;
    case kRangeSelection:
      script->setInnerHTML(
          // If the caret position is set before firing 'compositonend' event
          // (and it should), the final caret position will be reset to [2,4].
          "document.getElementById('sample').addEventListener('compositionend',"
          "  event => {"
          "    const node = document.getElementById('sample').firstChild;"
          "    const selection = getSelection();"
          "    selection.collapse(node, 2);"
          "    selection.extend(node, 4);"
          "});");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();
  editable->Focus();
}

TEST_F(InputMethodControllerTest, BackspaceFromEndOfInput) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  input->SetValue("fooX");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("fooX", input->Value());
  Controller().ExtendSelectionAndDelete(0, 0);
  EXPECT_EQ("fooX", input->Value());

  input->SetValue("fooX");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("fooX", input->Value());
  Controller().ExtendSelectionAndDelete(1, 0);
  EXPECT_EQ("foo", input->Value());

  input->SetValue(
      String::FromUTF8("foo\xE2\x98\x85"));  // U+2605 == "black star"
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("foo\xE2\x98\x85", input->Value().Utf8());
  Controller().ExtendSelectionAndDelete(1, 0);
  EXPECT_EQ("foo", input->Value());

  input->SetValue(
      String::FromUTF8("foo\xF0\x9F\x8F\x86"));  // U+1F3C6 == "trophy"
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("foo\xF0\x9F\x8F\x86", input->Value().Utf8());
  Controller().ExtendSelectionAndDelete(1, 0);
  EXPECT_EQ("foo", input->Value());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho"
  input->SetValue(String::FromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->Value().Utf8());
  Controller().ExtendSelectionAndDelete(1, 0);
  EXPECT_EQ("foo", input->Value());

  input->SetValue("fooX");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("fooX", input->Value());
  Controller().ExtendSelectionAndDelete(0, 1);
  EXPECT_EQ("fooX", input->Value());
}

TEST_F(InputMethodControllerTest, SetCompositionFromExistingText) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>hello world</div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 0, 5);

  Range* range = GetCompositionRange();
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(5u, range->endOffset());

  PlainTextRange plain_text_range(PlainTextRange::Create(*div, *range));
  EXPECT_EQ(0u, plain_text_range.Start());
  EXPECT_EQ(5u, plain_text_range.End());
}

TEST_F(InputMethodControllerTest, AddImeTextSpansToExistingText) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kAutocorrect, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().AddImeTextSpansToExistingText(ime_text_spans, 0, 5);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());
  EXPECT_EQ(DocumentMarker::MarkerType::kSuggestion,
            GetDocument().Markers().Markers()[0]->GetType());
  EXPECT_EQ(SuggestionMarker::SuggestionType::kAutocorrect,
            To<SuggestionMarker>(GetDocument().Markers().Markers()[0].Get())
                ->GetSuggestionType());
}

TEST_F(InputMethodControllerTest, AddGrammarCheckSpans) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)));

  Vector<ImeTextSpan> grammar_ime_text_spans;
  grammar_ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kGrammarSuggestion, 3, 6, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  grammar_ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kGrammarSuggestion, 8, 10, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  Controller().AddImeTextSpansToExistingText(grammar_ime_text_spans, 0, 10);
  // The first grammar check span should not be added because it overlaps with
  // the existing spellcheck span.
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());
  EXPECT_EQ(DocumentMarker::MarkerType::kSpelling,
            GetDocument().Markers().Markers()[0]->GetType());
  EXPECT_EQ(8u, GetDocument().Markers().Markers()[1]->StartOffset());
  EXPECT_EQ(10u, GetDocument().Markers().Markers()[1]->EndOffset());
  EXPECT_EQ(DocumentMarker::MarkerType::kSuggestion,
            GetDocument().Markers().Markers()[1]->GetType());
}

TEST_F(InputMethodControllerTest, GetImeTextSpans) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");
  ImeTextSpan span1 = ImeTextSpan(ImeTextSpan::Type::kAutocorrect, 0, 5,
                                  Color(255, 0, 0), ImeTextSpanThickness::kThin,
                                  ImeTextSpanUnderlineStyle::kSolid,
                                  Color::kTransparent, Color::kTransparent);
  ImeTextSpan span2 = ImeTextSpan(ImeTextSpan::Type::kComposition, 1, 3,
                                  Color(255, 0, 0), ImeTextSpanThickness::kThin,
                                  ImeTextSpanUnderlineStyle::kSolid,
                                  Color::kTransparent, Color::kTransparent);
  ImeTextSpan span3 = ImeTextSpan(
      ImeTextSpan::Type::kMisspellingSuggestion, 1, 3, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent);
  ImeTextSpan span4 = ImeTextSpan(
      ImeTextSpan::Type::kGrammarSuggestion, 6, 8, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent, Color::kTransparent, false,
      false, {String("fake_suggestion")});

  Controller().AddImeTextSpansToExistingText({span1, span2, span3, span4}, 0,
                                             10);
  Controller().SetEditableSelectionOffsets(PlainTextRange(1, 1));

  const WebVector<ui::ImeTextSpan>& ime_text_spans =
      Controller().TextInputInfo().ime_text_spans;

  EXPECT_EQ(2u, ime_text_spans.size());
  EXPECT_EQ(0u, ime_text_spans[0].start_offset);
  EXPECT_EQ(5u, ime_text_spans[0].end_offset);
  EXPECT_EQ(ui::ImeTextSpan::Type::kAutocorrect, ime_text_spans[0].type);
  EXPECT_EQ(0u, ime_text_spans[0].suggestions.size());

  EXPECT_EQ(6u, ime_text_spans[1].start_offset);
  EXPECT_EQ(8u, ime_text_spans[1].end_offset);
  EXPECT_EQ(ui::ImeTextSpan::Type::kGrammarSuggestion, ime_text_spans[1].type);
  EXPECT_EQ(1u, ime_text_spans[1].suggestions.size());
  EXPECT_EQ("fake_suggestion", ime_text_spans[1].suggestions[0]);
}

TEST_F(InputMethodControllerTest, SetCompositionAfterEmoji) {
  // "trophy" = U+1F3C6 = 0xF0 0x9F 0x8F 0x86 (UTF8).
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>&#x1f3c6</div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 2, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_EQ(2, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
  EXPECT_EQ(2, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Focus()
                   .ComputeOffsetInContainerNode());

  Controller().SetComposition(String("a"), ime_text_spans, 1, 1);
  EXPECT_EQ("\xF0\x9F\x8F\x86\x61", div->innerText().Utf8());

  Controller().SetComposition(String("ab"), ime_text_spans, 2, 2);
  EXPECT_EQ("\xF0\x9F\x8F\x86\x61\x62", div->innerText().Utf8());
}

TEST_F(InputMethodControllerTest, SetCompositionWithGraphemeCluster) {
  InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 6, 6, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // UTF16 = 0x0939 0x0947 0x0932 0x0932. Note that 0x0932 0x0932 is a grapheme
  // cluster.
  Controller().SetComposition(
      String::FromUTF8("\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA4\xB2"),
      ime_text_spans, 4, 4);
  EXPECT_EQ(4u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(4u, Controller().GetSelectionOffsets().End());

  // UTF16 = 0x0939 0x0947 0x0932 0x094D 0x0932 0x094B.
  Controller().SetComposition(
      String::FromUTF8("\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA5\x8D\xE0"
                       "\xA4\xB2\xE0\xA5\x8B"),
      ime_text_spans, 6, 6);
  EXPECT_EQ(6u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(6u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest,
       SetCompositionWithGraphemeClusterAndMultipleNodes) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 12, 12, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // UTF16 = 0x0939 0x0947 0x0932 0x094D 0x0932 0x094B. 0x0939 0x0947 0x0932 is
  // a grapheme cluster, so is the remainding 0x0932 0x094B.
  Controller().CommitText(
      String::FromUTF8("\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA5\x8D\xE0"
                       "\xA4\xB2\xE0\xA5\x8B"),
      ime_text_spans, 1);
  Controller().CommitText("\nab ", ime_text_spans, 1);
  Controller().SetComposition(String("c"), ime_text_spans, 1, 1);
  EXPECT_EQ(
      "\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA5\x8D\xE0\xA4\xB2\xE0\xA5"
      "\x8B\nab c",
      div->innerText().Utf8());
  EXPECT_EQ(11u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(11u, Controller().GetSelectionOffsets().End());

  Controller().SetComposition(String("cd"), ime_text_spans, 2, 2);
  EXPECT_EQ(
      "\xE0\xA4\xB9\xE0\xA5\x87\xE0\xA4\xB2\xE0\xA5\x8D\xE0\xA4\xB2\xE0\xA5"
      "\x8B\nab cd",
      div->innerText().Utf8());
  EXPECT_EQ(12u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(12u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest, SetCompositionKeepingStyle) {
  Element* div = InsertHTMLElement(
      "<div id='sample' "
      "contenteditable>abc1<b>2</b>34567<b>8</b>9d<b>e</b>f</div>",
      "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 3, 12, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 3, 12);

  // Subtract a character.
  Controller().SetComposition(String("12345789"), ime_text_spans, 8, 8);
  EXPECT_EQ("abc1<b>2</b>3457<b>8</b>9d<b>e</b>f", div->innerHTML().Utf8());
  EXPECT_EQ(11u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(11u, Controller().GetSelectionOffsets().End());

  // Append a character.
  Controller().SetComposition(String("123456789"), ime_text_spans, 9, 9);
  EXPECT_EQ("abc1<b>2</b>34567<b>8</b>9d<b>e</b>f", div->innerHTML().Utf8());
  EXPECT_EQ(12u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(12u, Controller().GetSelectionOffsets().End());

  // Subtract and append characters.
  Controller().SetComposition(String("123hello789"), ime_text_spans, 11, 11);
  EXPECT_EQ("abc1<b>2</b>3hello7<b>8</b>9d<b>e</b>f", div->innerHTML().Utf8());
}

TEST_F(InputMethodControllerTest, SetCompositionWithEmojiKeepingStyle) {
  // U+1F3E0 = 0xF0 0x9F 0x8F 0xA0 (UTF8). It's an emoji character.
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable><b>&#x1f3e0</b></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 2, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  Controller().SetCompositionFromExistingText(ime_text_spans, 0, 2);

  // 0xF0 0x9F 0x8F 0xAB is also an emoji character, with the same leading
  // surrogate pair to the previous one.
  Controller().SetComposition(String::FromUTF8("\xF0\x9F\x8F\xAB"),
                              ime_text_spans, 2, 2);
  EXPECT_EQ("<b>\xF0\x9F\x8F\xAB</b>", div->innerHTML().Utf8());

  Controller().SetComposition(String::FromUTF8("\xF0\x9F\x8F\xA0"),
                              ime_text_spans, 2, 2);
  EXPECT_EQ("<b>\xF0\x9F\x8F\xA0</b>", div->innerHTML().Utf8());
}

TEST_F(InputMethodControllerTest,
       SetCompositionWithTeluguSignVisargaKeepingStyle) {
  // U+0C03 = 0xE0 0xB0 0x83 (UTF8), a telugu sign visarga with one code point.
  // It's one grapheme cluster if separated. It can also form one grapheme
  // cluster with another code point(e.g, itself).
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable><b>&#xc03</b></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 2, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 0, 1);

  // 0xE0 0xB0 0x83 0xE0 0xB0 0x83, a telugu character with 2 code points in
  // 1 grapheme cluster.
  Controller().SetComposition(String::FromUTF8("\xE0\xB0\x83\xE0\xB0\x83"),
                              ime_text_spans, 2, 2);
  EXPECT_EQ("<b>\xE0\xB0\x83\xE0\xB0\x83</b>", div->innerHTML().Utf8());

  Controller().SetComposition(String::FromUTF8("\xE0\xB0\x83"), ime_text_spans,
                              1, 1);
  EXPECT_EQ("<b>\xE0\xB0\x83</b>", div->innerHTML().Utf8());
}

TEST_F(InputMethodControllerTest, FinishComposingTextKeepingStyle) {
  Element* div = InsertHTMLElement(
      "<div id='sample' "
      "contenteditable>abc1<b>2</b>34567<b>8</b>9</div>",
      "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 3, 12, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 3, 12);

  Controller().SetComposition(String("123hello789"), ime_text_spans, 11, 11);
  EXPECT_EQ("abc1<b>2</b>3hello7<b>8</b>9", div->innerHTML());

  Controller().FinishComposingText(InputMethodController::kKeepSelection);
  EXPECT_EQ("abc1<b>2</b>3hello7<b>8</b>9", div->innerHTML());
}

TEST_F(InputMethodControllerTest, FinishComposingTextKeepingBackwardSelection) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>|abc^</div>"),
      SetSelectionOptions());

  Controller().FinishComposingText(InputMethodController::kKeepSelection);

  EXPECT_EQ("<div contenteditable>|abc^</div>", GetSelectionTextFromBody());
}

TEST_F(InputMethodControllerTest, CommitTextKeepingStyle) {
  Element* div = InsertHTMLElement(
      "<div id='sample' "
      "contenteditable>abc1<b>2</b>34567<b>8</b>9</div>",
      "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 3, 12, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 3, 12);

  Controller().CommitText(String("123789"), ime_text_spans, 0);
  EXPECT_EQ("abc1<b>2</b>37<b>8</b>9", div->innerHTML());
}

TEST_F(InputMethodControllerTest, InsertTextWithNewLine) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 11, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  Controller().CommitText(String("hello\nworld"), ime_text_spans, 0);
  EXPECT_EQ("hello<div>world</div>", div->innerHTML());
}

TEST_F(InputMethodControllerTest, InsertTextWithNewLineIncrementally) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  Controller().CommitText("a", ime_text_spans, 0);
  Controller().SetComposition("bcd", ime_text_spans, 0, 2);
  EXPECT_EQ("abcd", div->innerHTML());

  Controller().CommitText(String("bcd\nefgh\nijkl"), ime_text_spans, 0);
  EXPECT_EQ("abcd<div>efgh</div><div>ijkl</div>", div->innerHTML());
}

TEST_F(InputMethodControllerTest, SelectionOnConfirmExistingText) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 0, 5);

  Controller().FinishComposingText(InputMethodController::kKeepSelection);
  EXPECT_EQ(0, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
  EXPECT_EQ(0, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Focus()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest, DeleteBySettingEmptyComposition) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  input->SetValue("foo ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("foo ", input->Value());
  Controller().ExtendSelectionAndDelete(0, 0);
  EXPECT_EQ("foo ", input->Value());

  input->SetValue("foo ");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("foo ", input->Value());
  Controller().ExtendSelectionAndDelete(1, 0);
  EXPECT_EQ("foo", input->Value());

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 3, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 0, 3);

  Controller().SetComposition(String(""), ime_text_spans, 0, 3);

  EXPECT_EQ("", input->Value());
}

TEST_F(InputMethodControllerTest,
       SetCompositionFromExistingTextWithCollapsedWhiteSpace) {
  // Creates a div with one leading new line char. The new line char is hidden
  // from the user and IME, but is visible to InputMethodController.
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>\nhello world</div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 0, 5);

  Range* range = GetCompositionRange();
  EXPECT_EQ(1u, range->startOffset());
  EXPECT_EQ(6u, range->endOffset());

  PlainTextRange plain_text_range(PlainTextRange::Create(*div, *range));
  EXPECT_EQ(0u, plain_text_range.Start());
  EXPECT_EQ(5u, plain_text_range.End());
}

TEST_F(InputMethodControllerTest,
       SetCompositionFromExistingTextWithInvalidOffsets) {
  InsertHTMLElement("<div id='sample' contenteditable>test</div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 7, 8, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(ime_text_spans, 7, 8);

  EXPECT_FALSE(GetCompositionRange());
}

TEST_F(InputMethodControllerTest, ConfirmPasswordComposition) {
  auto* input = To<HTMLInputElement>(InsertHTMLElement(
      "<input id='sample' type='password' size='24'>", "sample"));

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetComposition("foo", ime_text_spans, 0, 3);
  Controller().FinishComposingText(InputMethodController::kKeepSelection);

  EXPECT_EQ("foo", input->Value());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithEmptyText) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  input->SetValue("");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("", input->Value());
  Controller().DeleteSurroundingText(0, 0);
  EXPECT_EQ("", input->Value());

  input->SetValue("");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("", input->Value());
  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("", input->Value());

  input->SetValue("");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("", input->Value());
  Controller().DeleteSurroundingText(0, 1);
  EXPECT_EQ("", input->Value());

  input->SetValue("");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("", input->Value());
  Controller().DeleteSurroundingText(1, 1);
  EXPECT_EQ("", input->Value());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithRangeSelection) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(1, 4));
  Controller().DeleteSurroundingText(0, 0);
  EXPECT_EQ("hello", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(1, 4));
  Controller().DeleteSurroundingText(1, 1);
  EXPECT_EQ("ell", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(1, 4));
  Controller().DeleteSurroundingText(100, 0);
  EXPECT_EQ("ello", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(1, 4));
  Controller().DeleteSurroundingText(0, 100);
  EXPECT_EQ("hell", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(1, 4));
  Controller().DeleteSurroundingText(100, 100);
  EXPECT_EQ("ell", input->Value());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithCursorSelection) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("hllo", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  Controller().DeleteSurroundingText(0, 1);
  EXPECT_EQ("helo", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  Controller().DeleteSurroundingText(0, 0);
  EXPECT_EQ("hello", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  Controller().DeleteSurroundingText(1, 1);
  EXPECT_EQ("hlo", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  Controller().DeleteSurroundingText(100, 0);
  EXPECT_EQ("llo", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  Controller().DeleteSurroundingText(0, 100);
  EXPECT_EQ("he", input->Value());

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("hello", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  Controller().DeleteSurroundingText(100, 100);
  EXPECT_EQ("", input->Value());

  input->SetValue("h");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("h", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(1, 1));
  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("", input->Value());

  input->SetValue("h");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("h", input->Value());
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  Controller().DeleteSurroundingText(0, 1);
  EXPECT_EQ("", input->Value());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnTheLeft) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // U+2605 == "black star". It takes up 1 space.
  input->SetValue(String::FromUTF8("foo\xE2\x98\x85"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("foo\xE2\x98\x85", input->Value().Utf8());
  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("foo", input->Value());

  // U+1F3C6 == "trophy". It takes up 2 space.
  input->SetValue(String::FromUTF8("foo\xF0\x9F\x8F\x86"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(5, 5));
  EXPECT_EQ("foo\xF0\x9F\x8F\x86", input->Value().Utf8());
  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("foo\xED\xA0\xBC", input->Value().Utf8());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho". It takes up 2 space.
  input->SetValue(String::FromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(5, 5));
  EXPECT_EQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->Value().Utf8());
  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("foo\xE0\xB8\x81", input->Value().Utf8());

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_EQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86", input->Value().Utf8());
  Controller().DeleteSurroundingText(2, 0);
  EXPECT_EQ("foo\xF0\x9F\x8F\x86", input->Value().Utf8());

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_EQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86", input->Value().Utf8());
  Controller().DeleteSurroundingText(3, 0);
  EXPECT_EQ("foo\xED\xA0\xBC", input->Value().Utf8());

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_EQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86", input->Value().Utf8());
  Controller().DeleteSurroundingText(4, 0);
  EXPECT_EQ("foo", input->Value());

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_EQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86", input->Value().Utf8());
  Controller().DeleteSurroundingText(5, 0);
  EXPECT_EQ("fo", input->Value());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnTheRight) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // U+2605 == "black star". It takes up 1 space.
  input->SetValue(String::FromUTF8("\xE2\x98\x85 foo"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_EQ("\xE2\x98\x85 foo", input->Value().Utf8());
  Controller().DeleteSurroundingText(0, 1);
  EXPECT_EQ(" foo", input->Value());

  // U+1F3C6 == "trophy". It takes up 2 space.
  input->SetValue(String::FromUTF8("\xF0\x9F\x8F\x86 foo"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_EQ("\xF0\x9F\x8F\x86 foo", input->Value().Utf8());
  Controller().DeleteSurroundingText(0, 1);
  EXPECT_EQ("\xED\xBF\x86 foo", input->Value().Utf8());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho". It takes up 2 space.
  input->SetValue(String::FromUTF8("\xE0\xB8\x81\xE0\xB9\x89 foo"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_EQ("\xE0\xB8\x81\xE0\xB9\x89 foo", input->Value().Utf8());
  Controller().DeleteSurroundingText(0, 1);
  EXPECT_EQ("\xE0\xB9\x89 foo", input->Value().Utf8());

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_EQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo", input->Value().Utf8());
  Controller().DeleteSurroundingText(0, 2);
  EXPECT_EQ("\xF0\x9F\x8F\x86 foo", input->Value().Utf8());

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_EQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo", input->Value().Utf8());
  Controller().DeleteSurroundingText(0, 3);
  EXPECT_EQ("\xED\xBF\x86 foo", input->Value().Utf8());

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_EQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo", input->Value().Utf8());
  Controller().DeleteSurroundingText(0, 4);
  EXPECT_EQ(" foo", input->Value());

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_EQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo", input->Value().Utf8());
  Controller().DeleteSurroundingText(0, 5);
  EXPECT_EQ("foo", input->Value());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnBothSides) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // "trophy" + "trophy".
  input->SetValue(String::FromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_EQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86", input->Value().Utf8());
  Controller().DeleteSurroundingText(1, 1);
  // Deleted second half of the first trophy and the first half of the second
  // trophy, so we ended up with a complete trophy.
  EXPECT_EQ("\xF0\x9F\x8F\x86", input->Value().Utf8());
}

// This test comes from http://crbug.com/1024738. It is basically the same to
// composed text (U+0E01 "ka kai" + U+0E49 "mai tho"), but easier to understand.
TEST_F(InputMethodControllerTest, DeleteSurroundingTextForComposedCharacter) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));
  // p̂p̂ (U+0070 U+0302 U+0070 U+0302)
  input->SetValue(String::FromUTF8("\x70\xCC\x82\x70\xCC\x82"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_EQ("\x70\xCC\x82\x70\xCC\x82", input->Value().Utf8());
  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("\x70\xCC\x82\x70", input->Value().Utf8());
  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("\x70\xCC\x82", input->Value().Utf8());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextForMultipleNodes) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>aaa"
      "<div id='sample2' contenteditable>bbb"
      "<div id='sample3' contenteditable>ccc"
      "<div id='sample4' contenteditable>ddd"
      "<div id='sample5' contenteditable>eee"
      "</div></div></div></div></div>",
      "sample");

  Controller().SetEditableSelectionOffsets(PlainTextRange(8, 8));
  EXPECT_EQ("aaa\nbbb\nccc\nddd\neee", div->innerText());
  EXPECT_EQ(8u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(8u, Controller().GetSelectionOffsets().End());

  Controller().DeleteSurroundingText(1, 0);
  EXPECT_EQ("aaa\nbbbccc\nddd\neee", div->innerText());
  EXPECT_EQ(7u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(7u, Controller().GetSelectionOffsets().End());

  Controller().DeleteSurroundingText(0, 4);
  EXPECT_EQ("aaa\nbbbddd\neee", div->innerText());
  EXPECT_EQ(7u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(7u, Controller().GetSelectionOffsets().End());

  Controller().DeleteSurroundingText(5, 5);
  EXPECT_EQ("aaee", div->innerText());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextInCodePointsWithMultiCodeTextOnTheLeft) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text (U+0E01
  // "ka kai" + U+0E49 "mai tho").
  // A "black star" is 1 grapheme cluster. It has 1 code point, and its length
  // is 1 (abbreviated as [1,1,1]). A "trophy": [1,1,2]. The composed text:
  // [1,2,2].
  input->SetValue(String::FromUTF8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // The cursor is at the end of the text.
  Controller().SetEditableSelectionOffsets(PlainTextRange(8, 8));

  Controller().DeleteSurroundingTextInCodePoints(2, 0);
  EXPECT_EQ("a\xE2\x98\x85 \xF0\x9F\x8F\x86 ", input->Value().Utf8());
  Controller().DeleteSurroundingTextInCodePoints(4, 0);
  EXPECT_EQ("a", input->Value());

  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text
  input->SetValue(String::FromUTF8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // The cursor is at the end of the text.
  Controller().SetEditableSelectionOffsets(PlainTextRange(8, 8));

  // We should only delete 1 code point.
  Controller().DeleteSurroundingTextInCodePoints(1, 0);
  EXPECT_EQ("a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81",
            input->Value().Utf8());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextInCodePointsWithMultiCodeTextOnTheRight) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text
  input->SetValue(String::FromUTF8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));

  Controller().DeleteSurroundingTextInCodePoints(0, 5);
  EXPECT_EQ("\xE0\xB8\x81\xE0\xB9\x89", input->Value().Utf8());

  Controller().DeleteSurroundingTextInCodePoints(0, 1);
  // We should only delete 1 code point.
  EXPECT_EQ("\xE0\xB9\x89", input->Value().Utf8());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextInCodePointsWithMultiCodeTextOnBothSides) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // 'a' + "black star" + SPACE + "trophy" + SPACE + composed text
  input->SetValue(String::FromUTF8(
      "a\xE2\x98\x85 \xF0\x9F\x8F\x86 \xE0\xB8\x81\xE0\xB9\x89"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(3, 3));
  Controller().DeleteSurroundingTextInCodePoints(2, 2);
  EXPECT_EQ("a\xE0\xB8\x81\xE0\xB9\x89", input->Value().Utf8());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextInCodePointsWithImage) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>aaa"
      "<img src='empty.png'>bbb</div>",
      "sample");

  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));
  Controller().DeleteSurroundingTextInCodePoints(1, 1);
  EXPECT_EQ("aaabb", div->innerText());
  EXPECT_EQ(3u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(3u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextInCodePointsWithInvalidSurrogatePair) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // 'a' + high surrogate of "trophy" + "black star" + low surrogate of "trophy"
  // + SPACE
  const UChar kUText[] = {'a', 0xD83C, 0x2605, 0xDFC6, ' ', '\0'};
  const String& text = String(kUText);

  input->SetValue(text);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  // The invalid high surrogate is encoded as '\xED\xA0\xBC', and invalid low
  // surrogate is encoded as '\xED\xBF\x86'.
  EXPECT_EQ("a\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86 ", input->Value().Utf8());

  Controller().SetEditableSelectionOffsets(PlainTextRange(5, 5));
  // Delete a SPACE.
  Controller().DeleteSurroundingTextInCodePoints(1, 0);
  EXPECT_EQ("a\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86", input->Value().Utf8());
  // Do nothing since there is an invalid surrogate in the requested range.
  Controller().DeleteSurroundingTextInCodePoints(2, 0);
  EXPECT_EQ("a\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86", input->Value().Utf8());

  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 0));
  // Delete 'a'.
  Controller().DeleteSurroundingTextInCodePoints(0, 1);
  EXPECT_EQ("\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86", input->Value().Utf8());
  // Do nothing since there is an invalid surrogate in the requested range.
  Controller().DeleteSurroundingTextInCodePoints(0, 2);
  EXPECT_EQ("\xED\xA0\xBC\xE2\x98\x85\xED\xBF\x86", input->Value().Utf8());
}

TEST_F(InputMethodControllerTest, ReplaceTextAndDoNotChangeSelection) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // The replaced range does not overlap with the selection range.
  input->SetValue("Hello world!");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("Hello world!", input->Value());
  // Select "world!".
  Controller().SetEditableSelectionOffsets(PlainTextRange(6, 12));
  // Replace "Hello" with "Hi".
  Controller().ReplaceTextAndMoveCaret(
      "Hi", PlainTextRange(0, 5),
      InputMethodController::MoveCaretBehavior::kDoNotMove);
  EXPECT_EQ("Hi world!", input->Value());
  // The selection is still "world!".
  EXPECT_EQ(3u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(9u, Controller().GetSelectionOffsets().End());

  // The replaced range is the same as the selection range.
  input->SetValue("Hello world!");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("Hello world!", input->Value());
  // Select "Hello".
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 5));
  // Replace "Hello" with "Hi".
  Controller().ReplaceTextAndMoveCaret(
      "Hi", PlainTextRange(0, 5),
      InputMethodController::MoveCaretBehavior::kDoNotMove);
  EXPECT_EQ("Hi world!", input->Value());

  // The new selection is "Hi".
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().End());

  // The replaced range partially overlaps with the selection range.
  input->SetValue("Hello world!");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("Hello world!", input->Value());
  // Select "Hello".
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 5));
  // Replace "He" with "Hi".
  Controller().ReplaceTextAndMoveCaret(
      "Hi", PlainTextRange(0, 2),
      InputMethodController::MoveCaretBehavior::kDoNotMove);
  EXPECT_EQ("Hillo world!", input->Value());
  // The selection is still "Hillo".
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(5u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest,
       ReplaceTextAndMoveCursorAfterTheReplacementText) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  // The caret should always move to the end of the replacement text no matter
  // where the current selection is.

  input->SetValue("Good morning!");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("Good morning!", input->Value());
  // Select "Good".
  Controller().SetEditableSelectionOffsets(PlainTextRange(0, 4));
  // Replace "morning" with "night". The replaced range does not overlap with
  // the selection range.
  Controller().ReplaceTextAndMoveCaret(
      "night", PlainTextRange(5, 12),
      InputMethodController::MoveCaretBehavior::kMoveCaretAfterText);
  EXPECT_EQ("Good night!", input->Value());
  // The caret should be after "night".
  EXPECT_EQ(10u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(10u, Controller().GetSelectionOffsets().End());

  input->SetValue("Good morning!");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("Good morning!", input->Value());
  // Select "morning".
  Controller().SetEditableSelectionOffsets(PlainTextRange(5, 12));
  // Replace "morning" with "night". The replaced range is the same as the
  // selection range.
  Controller().ReplaceTextAndMoveCaret(
      "night", PlainTextRange(5, 12),
      InputMethodController::MoveCaretBehavior::kMoveCaretAfterText);
  EXPECT_EQ("Good night!", input->Value());
  // The caret should be after "night".
  EXPECT_EQ(10u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(10u, Controller().GetSelectionOffsets().End());

  input->SetValue("Good morning!");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ("Good morning!", input->Value());
  // Select "d mo".
  Controller().SetEditableSelectionOffsets(PlainTextRange(3, 7));
  // Replace "morning" with "night". The replaced range partially overlaps with
  // the selection range.
  Controller().ReplaceTextAndMoveCaret(
      "night", PlainTextRange(5, 12),
      InputMethodController::MoveCaretBehavior::kMoveCaretAfterText);
  EXPECT_EQ("Good night!", input->Value());
  // The caret should be after "night".
  EXPECT_EQ(10u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(10u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest, SetCompositionForInputWithNewCaretPositions) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  input->SetValue("hello");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_EQ("hello", input->Value());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().End());

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 2, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  // The caret exceeds left boundary.
  // "*heABllo", where * stands for caret.
  Controller().SetComposition("AB", ime_text_spans, -100, -100);
  EXPECT_EQ("heABllo", input->Value());
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().End());

  // The caret is on left boundary.
  // "*heABllo".
  Controller().SetComposition("AB", ime_text_spans, -2, -2);
  EXPECT_EQ("heABllo", input->Value());
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().End());

  // The caret is before the composing text.
  // "he*ABllo".
  Controller().SetComposition("AB", ime_text_spans, 0, 0);
  EXPECT_EQ("heABllo", input->Value());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().End());

  // The caret is after the composing text.
  // "heAB*llo".
  Controller().SetComposition("AB", ime_text_spans, 2, 2);
  EXPECT_EQ("heABllo", input->Value());
  EXPECT_EQ(4u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(4u, Controller().GetSelectionOffsets().End());

  // The caret is on right boundary.
  // "heABllo*".
  Controller().SetComposition("AB", ime_text_spans, 5, 5);
  EXPECT_EQ("heABllo", input->Value());
  EXPECT_EQ(7u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(7u, Controller().GetSelectionOffsets().End());

  // The caret exceeds right boundary.
  // "heABllo*".
  Controller().SetComposition("AB", ime_text_spans, 100, 100);
  EXPECT_EQ("heABllo", input->Value());
  EXPECT_EQ(7u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(7u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest,
       SetCompositionForContentEditableWithNewCaretPositions) {
  // There are 7 nodes and 5+1+5+1+3+4+3 characters: "hello", '\n', "world",
  // "\n", "012", "3456", "789".
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>"
      "hello"
      "<div id='sample2' contenteditable>world"
      "<p>012<b>3456</b><i>789</i></p>"
      "</div>"
      "</div>",
      "sample");

  Controller().SetEditableSelectionOffsets(PlainTextRange(17, 17));
  EXPECT_EQ("hello\nworld\n\n0123456789", div->innerText());
  EXPECT_EQ(17u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(17u, Controller().GetSelectionOffsets().End());

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 2, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  // The caret exceeds left boundary.
  // "*hello\nworld\n\n01234AB56789", where * stands for caret.
  Controller().SetComposition("AB", ime_text_spans, -100, -100);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().End());

  // The caret is on left boundary.
  // "*hello\nworld\n\n01234AB56789".
  Controller().SetComposition("AB", ime_text_spans, -17, -17);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(0u, Controller().GetSelectionOffsets().End());

  // The caret is in the 1st node.
  // "he*llo\nworld\n\n01234AB56789".
  Controller().SetComposition("AB", ime_text_spans, -15, -15);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().End());

  // The caret is on right boundary of the 1st node.
  // "hello*\nworld\n\n01234AB56789".
  Controller().SetComposition("AB", ime_text_spans, -12, -12);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(5u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(5u, Controller().GetSelectionOffsets().End());

  // The caret is on right boundary of the 2nd node.
  // "hello\n*world\n\n01234AB56789".
  Controller().SetComposition("AB", ime_text_spans, -11, -11);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(6u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(6u, Controller().GetSelectionOffsets().End());

  // The caret is on right boundary of the 3rd node.
  // "hello\nworld*\n01234AB56789".
  Controller().SetComposition("AB", ime_text_spans, -6, -6);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(11u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(11u, Controller().GetSelectionOffsets().End());

  // The caret is on right boundary of the 4th node.
  // "hello\nworld\n*01234AB56789".
  Controller().SetComposition("AB", ime_text_spans, -5, -5);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(12u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(12u, Controller().GetSelectionOffsets().End());

  // The caret is before the composing text.
  // "hello\nworld\n\n01234*AB56789".
  Controller().SetComposition("AB", ime_text_spans, 0, 0);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(17u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(17u, Controller().GetSelectionOffsets().End());

  // The caret is after the composing text.
  // "hello\nworld\n\n01234AB*56789".
  Controller().SetComposition("AB", ime_text_spans, 2, 2);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(19u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(19u, Controller().GetSelectionOffsets().End());

  // The caret is on right boundary.
  // "hello\nworld\n\n01234AB56789*".
  Controller().SetComposition("AB", ime_text_spans, 7, 7);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(24u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(24u, Controller().GetSelectionOffsets().End());

  // The caret exceeds right boundary.
  // "hello\nworld\n\n01234AB56789*".
  Controller().SetComposition("AB", ime_text_spans, 100, 100);
  EXPECT_EQ("hello\nworld\n\n01234AB56789", div->innerText());
  EXPECT_EQ(24u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(24u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest, SetCompositionWithEmptyText) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>hello</div>", "sample");

  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_EQ("hello", div->innerText());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().End());

  Vector<ImeTextSpan> ime_text_spans0;
  ime_text_spans0.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 0, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Vector<ImeTextSpan> ime_text_spans2;
  ime_text_spans2.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 2, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  Controller().SetComposition("AB", ime_text_spans2, 2, 2);
  // With previous composition.
  Controller().SetComposition("", ime_text_spans0, 2, 2);
  EXPECT_EQ("hello", div->innerText());
  EXPECT_EQ(4u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(4u, Controller().GetSelectionOffsets().End());

  // Without previous composition.
  Controller().SetComposition("", ime_text_spans0, -1, -1);
  EXPECT_EQ("hello", div->innerText());
  EXPECT_EQ(3u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(3u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest, InsertLineBreakWhileComposingText) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetComposition("hello", ime_text_spans, 5, 5);
  EXPECT_EQ("hello", div->innerText());
  EXPECT_EQ(5u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(5u, Controller().GetSelectionOffsets().End());

  GetFrame().GetEditor().InsertLineBreak();
  EXPECT_EQ("hello\n\n", div->innerText());
  EXPECT_EQ(6u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(6u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest, InsertLineBreakAfterConfirmingText) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 2, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().CommitText("hello", ime_text_spans, 0);
  EXPECT_EQ("hello", div->innerText());

  Controller().SetEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(2u, Controller().GetSelectionOffsets().End());

  GetFrame().GetEditor().InsertLineBreak();
  EXPECT_EQ("he\nllo", div->innerText());
  EXPECT_EQ(3u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(3u, Controller().GetSelectionOffsets().End());
}

TEST_F(InputMethodControllerTest, CompositionInputEventIsComposing) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* editable =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('beforeinput', "
      "  event => document.title = "
      "  `beforeinput.isComposing:${event.isComposing};`);"
      "document.getElementById('sample').addEventListener('input', "
      "  event => document.title += "
      "  `input.isComposing:${event.isComposing};`);");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Simulate composition in the |contentEditable|.
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  editable->Focus();

  GetDocument().setTitle(g_empty_string);
  Controller().SetComposition("foo", ime_text_spans, 0, 3);
  EXPECT_EQ("beforeinput.isComposing:true;input.isComposing:true;",
            GetDocument().title());

  GetDocument().setTitle(g_empty_string);
  Controller().CommitText("bar", ime_text_spans, 0);
  // Last pair of InputEvent should also be inside composition scope.
  EXPECT_EQ("beforeinput.isComposing:true;input.isComposing:true;",
            GetDocument().title());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForReplace) {
  CreateHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  GetDocument().setTitle(g_empty_string);
  Controller().SetComposition("hell", ime_text_spans, 4, 4);
  EXPECT_EQ(
      "beforeinput.data:hell;beforeinput.targetRanges:0-0;input.data:hell;",
      GetDocument().title());

  // Replace the existing composition.
  GetDocument().setTitle(g_empty_string);
  Controller().SetComposition("hello", ime_text_spans, 0, 0);
  EXPECT_EQ(
      "beforeinput.data:hello;beforeinput.targetRanges:0-4;input.data:hello;",
      GetDocument().title());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForConfirm) {
  CreateHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  GetDocument().setTitle(g_empty_string);
  Controller().SetComposition("hello", ime_text_spans, 5, 5);
  EXPECT_EQ(
      "beforeinput.data:hello;beforeinput.targetRanges:0-0;input.data:hello;",
      GetDocument().title());

  // Confirm the ongoing composition.
  GetDocument().setTitle(g_empty_string);
  Controller().FinishComposingText(InputMethodController::kKeepSelection);
  EXPECT_EQ("compositionend.data:hello;", GetDocument().title());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForDelete) {
  CreateHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  GetDocument().setTitle(g_empty_string);
  Controller().SetComposition("hello", ime_text_spans, 5, 5);
  EXPECT_EQ(
      "beforeinput.data:hello;beforeinput.targetRanges:0-0;input.data:hello;",
      GetDocument().title());

  // Delete the existing composition.
  GetDocument().setTitle(g_empty_string);
  Controller().SetComposition("", ime_text_spans, 0, 0);
  EXPECT_EQ(
      "beforeinput.data:;beforeinput.targetRanges:0-5;input.data:null;"
      "compositionend.data:;",
      GetDocument().title());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForInsert) {
  CreateHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  // Insert new text without previous composition.
  GetDocument().setTitle(g_empty_string);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().CommitText("hello", ime_text_spans, 0);
  EXPECT_EQ(
      "beforeinput.data:hello;beforeinput.targetRanges:0-0;input.data:hello;",
      GetDocument().title());

  GetDocument().setTitle(g_empty_string);
  Controller().SetComposition("n", ime_text_spans, 1, 1);
  EXPECT_EQ("beforeinput.data:n;beforeinput.targetRanges:5-5;input.data:n;",
            GetDocument().title());

  // Insert new text with previous composition.
  GetDocument().setTitle(g_empty_string);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().CommitText("hello", ime_text_spans, 1);
  EXPECT_EQ(
      "beforeinput.data:hello;beforeinput.targetRanges:5-6;input.data:hello;"
      "compositionend.data:hello;",
      GetDocument().title());
}

TEST_F(InputMethodControllerTest, CompositionInputEventForInsertEmptyText) {
  CreateHTMLWithCompositionInputEventListeners();

  // Simulate composition in the |contentEditable|.
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  // Insert empty text without previous composition.
  GetDocument().setTitle(g_empty_string);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().CommitText("", ime_text_spans, 0);
  EXPECT_EQ("", GetDocument().title().Utf8());

  GetDocument().setTitle(g_empty_string);
  Controller().SetComposition("n", ime_text_spans, 1, 1);
  EXPECT_EQ("beforeinput.data:n;beforeinput.targetRanges:0-0;input.data:n;",
            GetDocument().title());

  // Insert empty text with previous composition.
  GetDocument().setTitle(g_empty_string);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().CommitText("", ime_text_spans, 1);
  EXPECT_EQ(
      "beforeinput.data:;beforeinput.targetRanges:0-1;input.data:null;"
      "compositionend.data:;",
      GetDocument().title());
}

TEST_F(InputMethodControllerTest, CompositionEndEventWithNoSelection) {
  CreateHTMLWithCompositionEndEventListener(kNoSelection);

  // Simulate composition in the |contentEditable|.
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  Controller().SetComposition("hello", ime_text_spans, 1, 1);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_EQ(1u, Controller().GetSelectionOffsets().Start());
  EXPECT_EQ(1u, Controller().GetSelectionOffsets().End());

  // Confirm the ongoing composition. Note that it moves the caret to the end of
  // text [5,5] before firing 'compositonend' event.
  Controller().FinishComposingText(InputMethodController::kDoNotKeepSelection);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  EXPECT_TRUE(Controller().GetSelectionOffsets().IsNull());
}

TEST_F(InputMethodControllerTest, FinishCompositionRemovedRange) {
  Element* input_a =
      InsertHTMLElement("<input id='a' /><br><input type='tel' id='b' />", "a");

  EXPECT_EQ(kWebTextInputTypeText, Controller().TextInputType());

  // The test requires non-empty composition.
  Controller().SetComposition("hello", Vector<ImeTextSpan>(), 5, 5);
  EXPECT_EQ(kWebTextInputTypeText, Controller().TextInputType());

  // Remove element 'a'.
  input_a->setOuterHTML("", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(kWebTextInputTypeNone, Controller().TextInputType());

  GetDocument().getElementById(AtomicString("b"))->Focus();
  EXPECT_EQ(kWebTextInputTypeTelephone, Controller().TextInputType());

  Controller().FinishComposingText(InputMethodController::kKeepSelection);
  EXPECT_EQ(kWebTextInputTypeTelephone, Controller().TextInputType());
}

TEST_F(InputMethodControllerTest, ReflectsSpaceWithoutNbspMangling) {
  InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  Controller().CommitText(String("  "), ime_text_spans, 0);

  // In a contenteditable, multiple spaces or a space at the edge needs to be
  // nbsp to affect layout properly, but it confuses some IMEs (particularly
  // Vietnamese, see crbug.com/663880) to have their spaces reflected back to
  // them as nbsp.
  EXPECT_EQ(' ', Controller().TextInputInfo().value.Ascii()[0]);
  EXPECT_EQ(' ', Controller().TextInputInfo().value.Ascii()[1]);
}

TEST_F(InputMethodControllerTest, SetCompositionPlainTextWithIme_Text_Span) {
  InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 1, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  Controller().SetComposition(" ", ime_text_spans, 1, 1);

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(1u, GetDocument().Markers().Markers()[0]->EndOffset());
}

TEST_F(InputMethodControllerTest,
       SetCompositionPlainTextWithIme_Text_Span_Interim_Char_Selection) {
  InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 1, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent, Color::kTransparent, false,
      true /*interim_char_selection*/));

  Controller().SetComposition("a", ime_text_spans, 0, 1);

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());

  auto* styleable_marker =
      DynamicTo<StyleableMarker>(GetDocument().Markers().Markers()[0].Get());
  EXPECT_EQ(ImeTextSpanUnderlineStyle::kSolid,
            styleable_marker->UnderlineStyle());
}

TEST_F(InputMethodControllerTest, CommitPlainTextWithIme_Text_SpanInsert) {
  InsertHTMLElement("<div id='sample' contenteditable>Initial text.</div>",
                    "sample");

  Vector<ImeTextSpan> ime_text_spans;

  Controller().SetEditableSelectionOffsets(PlainTextRange(8, 8));

  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 1, 11, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  Controller().CommitText(String("ime_text_spand"), ime_text_spans, 0);

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(9u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(19u, GetDocument().Markers().Markers()[0]->EndOffset());
}

TEST_F(InputMethodControllerTest, CommitPlainTextWithIme_Text_SpanReplace) {
  InsertHTMLElement("<div id='sample' contenteditable>Initial text.</div>",
                    "sample");

  Vector<ImeTextSpan> ime_text_spans;

  Controller().SetCompositionFromExistingText(ime_text_spans, 8, 12);

  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 1, 11, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));

  Controller().CommitText(String("string"), ime_text_spans, 0);

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(9u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(15u, GetDocument().Markers().Markers()[0]->EndOffset());
}

TEST_F(InputMethodControllerTest, ImeTextSpanAppearsCorrectlyAfterNewline) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  Controller().SetComposition(String("hello"), ime_text_spans, 6, 6);
  Controller().FinishComposingText(InputMethodController::kKeepSelection);
  GetFrame().GetEditor().InsertLineBreak();

  Controller().SetCompositionFromExistingText(ime_text_spans, 8, 8);

  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetComposition(String("world"), ime_text_spans, 0, 0);
  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());

  // Verify composition marker shows up on the second line, not the first
  const Position& first_line_position =
      PlainTextRange(2).CreateRange(*div).StartPosition();
  const Position& second_line_position =
      PlainTextRange(8).CreateRange(*div).StartPosition();
  ASSERT_EQ(
      0u, GetDocument()
              .Markers()
              .MarkersFor(To<Text>(*first_line_position.ComputeContainerNode()))
              .size());
  ASSERT_EQ(1u, GetDocument()
                    .Markers()
                    .MarkersFor(
                        To<Text>(*second_line_position.ComputeContainerNode()))
                    .size());

  // Verify marker has correct start/end offsets (measured from the beginning
  // of the node, which is the beginning of the line)
  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());
}

TEST_F(InputMethodControllerTest, SelectionWhenFocusChangeFinishesComposition) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* editable =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  editable->Focus();

  // Simulate composition in the |contentEditable|.
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 5, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetComposition("foo", ime_text_spans, 3, 3);

  EXPECT_TRUE(Controller().HasComposition());
  EXPECT_EQ(0u, GetCompositionRange()->startOffset());
  EXPECT_EQ(3u, GetCompositionRange()->endOffset());
  EXPECT_EQ(3, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());

  // Insert 'test'.
  NonThrowableExceptionState exception_state;
  GetDocument().execCommand("insertText", false, "test", exception_state);

  EXPECT_TRUE(Controller().HasComposition());
  EXPECT_EQ(7, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());

  // Focus change finishes composition.
  editable->blur();
  editable->Focus();

  // Make sure that caret is still at the end of the inserted text.
  EXPECT_FALSE(Controller().HasComposition());
  EXPECT_EQ(7, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest, SetEmptyCompositionShouldNotMoveCaret) {
  auto* textarea =
      To<HTMLTextAreaElement>(InsertHTMLElement("<textarea id='txt'>", "txt"));

  textarea->SetValue("abc\n");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 4));

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 3, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetComposition(String("def"), ime_text_spans, 0, 3);
  Controller().SetComposition(String(""), ime_text_spans, 0, 3);
  Controller().CommitText(String("def"), ime_text_spans, 0);

  EXPECT_EQ("abc\ndef", textarea->Value());
}

TEST_F(InputMethodControllerTest, WhitespaceFixup) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text blah</div>", "sample");

  // Delete "Initial"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 7);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // The space at the beginning of the string should have been converted to an
  // nbsp
  EXPECT_EQ("&nbsp;text blah", div->innerHTML());

  // Delete "blah"
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 6, 10);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // The space at the end of the string should have been converted to an nbsp
  EXPECT_EQ("&nbsp;text&nbsp;", div->innerHTML());
}

TEST_F(InputMethodControllerTest, CommitEmptyTextDeletesSelection) {
  auto* input =
      To<HTMLInputElement>(InsertHTMLElement("<input id='sample'>", "sample"));

  input->SetValue("Abc Def Ghi");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 8));
  Controller().CommitText(String(""), empty_ime_text_spans, 0);
  EXPECT_EQ("Abc Ghi", input->Value());

  Controller().SetEditableSelectionOffsets(PlainTextRange(4, 7));
  Controller().CommitText(String("1"), empty_ime_text_spans, 0);
  EXPECT_EQ("Abc 1", input->Value());
}

static String GetMarkedText(
    DocumentMarkerController& document_marker_controller,
    Node* node,
    int marker_index) {
  DocumentMarker* marker = document_marker_controller.Markers()[marker_index];
  return node->textContent().Substring(
      marker->StartOffset(), marker->EndOffset() - marker->StartOffset());
}

TEST_F(InputMethodControllerTest,
       Marker_WhitespaceFixupAroundContentIndependentMarkerNotContainingSpace) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text blah</div>", "sample");

  // Add marker under "text" (use TextMatch since Composition markers don't
  // persist across editing operations)
  EphemeralRange marker_range = PlainTextRange(8, 12).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);
  // Delete "Initial"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 7);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Delete "blah"
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 6, 10);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Check that the marker is still attached to "text" and doesn't include
  // either space around it
  EXPECT_EQ(
      1u,
      GetDocument().Markers().MarkersFor(To<Text>(*div->firstChild())).size());
  EXPECT_EQ("text",
            GetMarkedText(GetDocument().Markers(), div->firstChild(), 0));
}

TEST_F(InputMethodControllerTest,
       Marker_WhitespaceFixupAroundContentIndependentMarkerBeginningWithSpace) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text blah</div>", "sample");

  // Add marker under " text" (use TextMatch since Composition markers don't
  // persist across editing operations)
  EphemeralRange marker_range = PlainTextRange(7, 12).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);
  // Delete "Initial"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 7);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Delete "blah"
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 6, 10);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Check that the marker is still attached to " text" and includes the space
  // before "text" but not the space after
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_EQ(
      "\xC2\xA0text",
      GetMarkedText(GetDocument().Markers(), div->firstChild(), 0).Utf8());
}

TEST_F(InputMethodControllerTest,
       Marker_WhitespaceFixupAroundContentIndependentMarkerEndingWithSpace) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text blah</div>", "sample");

  // Add marker under "text " (use TextMatch since Composition markers don't
  // persist across editing operations)
  EphemeralRange marker_range = PlainTextRange(8, 13).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);
  // Delete "Initial"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 7);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Delete "blah"
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 6, 10);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Check that the marker is still attached to "text " and includes the space
  // after "text" but not the space before
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_EQ(
      "text\xC2\xA0",
      GetMarkedText(GetDocument().Markers(), div->firstChild(), 0).Utf8());
}

TEST_F(
    InputMethodControllerTest,
    Marker_WhitespaceFixupAroundContentIndependentMarkerBeginningAndEndingWithSpaces) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text blah</div>", "sample");

  // Add marker under " text " (use TextMatch since Composition markers don't
  // persist across editing operations)
  EphemeralRange marker_range = PlainTextRange(7, 13).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  // Delete "Initial"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 7);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Delete "blah"
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 6, 10);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Check that the marker is still attached to " text " and includes both the
  // space before "text" and the space after
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_EQ(
      "\xC2\xA0text\xC2\xA0",
      GetMarkedText(GetDocument().Markers(), div->firstChild(), 0).Utf8());
}

TEST_F(InputMethodControllerTest, ContentDependentMarker_ReplaceStartOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "Initial text"
  EphemeralRange marker_range = PlainTextRange(0, 12).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Replace "Initial" with "Original"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 7);
  Controller().CommitText(String("Original"), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_ReplaceStartOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "Initial text"
  EphemeralRange marker_range = PlainTextRange(0, 12).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  // Replace "Initial" with "Original"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 7);
  Controller().CommitText(String("Original"), empty_ime_text_spans, 0);

  // Verify marker is under "Original text"
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_EQ(
      "Original text",
      GetMarkedText(GetDocument().Markers(), div->firstChild(), 0).Utf8());
}

TEST_F(InputMethodControllerTest,
       ContentDependentMarker_ReplaceTextContainsStartOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>This is some initial text</div>",
      "sample");

  // Add marker under "initial text"
  EphemeralRange marker_range = PlainTextRange(13, 25).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Replace "some initial" with "boring"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 8, 20);
  Controller().CommitText(String("boring"), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_ReplaceTextContainsStartOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>This is some initial text</div>",
      "sample");

  // Add marker under "initial text"
  EphemeralRange marker_range = PlainTextRange(13, 25).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  // Replace "some initial" with "boring"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 8, 20);
  Controller().CommitText(String("boring"), empty_ime_text_spans, 0);

  // Verify marker is under " text"
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(" text",
            GetMarkedText(GetDocument().Markers(), div->firstChild(), 0));
}

TEST_F(InputMethodControllerTest, ContentDependentMarker_ReplaceEndOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "Initial text"
  EphemeralRange marker_range = PlainTextRange(0, 12).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Replace "text" with "string"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 8, 12);
  Controller().CommitText(String("string"), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest, ContentIndependentMarker_ReplaceEndOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "Initial text"
  EphemeralRange marker_range = PlainTextRange(0, 12).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  // Replace "text" with "string"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 8, 12);
  Controller().CommitText(String("string"), empty_ime_text_spans, 0);

  // Verify marker is under "Initial string"
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_EQ(
      "Initial string",
      GetMarkedText(GetDocument().Markers(), div->firstChild(), 0).Utf8());
}

TEST_F(InputMethodControllerTest,
       ContentDependentMarker_ReplaceTextContainsEndOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>This is some initial text</div>",
      "sample");

  // Add marker under "some initial"
  EphemeralRange marker_range = PlainTextRange(8, 20).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Replace "initial text" with "content"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 13, 25);
  Controller().CommitText(String("content"), empty_ime_text_spans, 0);

  EXPECT_EQ("This is some content", div->innerHTML());

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_ReplaceTextContainsEndOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>This is some initial text</div>",
      "sample");

  // Add marker under "some initial"
  EphemeralRange marker_range = PlainTextRange(8, 20).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  // Replace "initial text" with "content"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 13, 25);
  Controller().CommitText(String("content"), empty_ime_text_spans, 0);

  EXPECT_EQ("This is some content", div->innerHTML());

  // Verify marker is under "some "
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_EQ("some ",
            GetMarkedText(GetDocument().Markers(), div->firstChild(), 0));
}

TEST_F(InputMethodControllerTest, ContentDependentMarker_ReplaceEntireMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "text"
  EphemeralRange marker_range = PlainTextRange(8, 12).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Replace "text" with "string"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 8, 12);
  Controller().CommitText(String("string"), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_ReplaceEntireMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "text"
  EphemeralRange marker_range = PlainTextRange(8, 12).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  // Replace "text" with "string"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 8, 12);
  Controller().CommitText(String("string"), empty_ime_text_spans, 0);

  // Verify marker is under "string"
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_EQ(
      "string",
      GetMarkedText(GetDocument().Markers(), div->firstChild(), 0).Utf8());
}

TEST_F(InputMethodControllerTest,
       ContentDependentMarker_ReplaceTextWithMarkerAtBeginning) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "Initial"
  EphemeralRange marker_range = PlainTextRange(0, 7).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Replace "Initial text" with "New string"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 12);
  Controller().CommitText(String("New string"), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_ReplaceTextWithMarkerAtBeginning) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "Initial"
  EphemeralRange marker_range = PlainTextRange(0, 7).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Replace "Initial text" with "New string"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 12);
  Controller().CommitText(String("New string"), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentDependentMarker_ReplaceTextWithMarkerAtEnd) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "text"
  EphemeralRange marker_range = PlainTextRange(8, 12).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Replace "Initial text" with "New string"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 12);
  Controller().CommitText(String("New string"), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_ReplaceTextWithMarkerAtEnd) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>Initial text</div>", "sample");

  // Add marker under "text"
  EphemeralRange marker_range = PlainTextRange(8, 12).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Replace "Initial text" with "New string"
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 0, 12);
  Controller().CommitText(String("New string"), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest, ContentDependentMarker_Deletions) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(0, 5).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  marker_range = PlainTextRange(5, 10).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  marker_range = PlainTextRange(10, 15).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  marker_range = PlainTextRange(15, 20).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  marker_range = PlainTextRange(20, 25).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  EXPECT_EQ(5u, GetDocument().Markers().Markers().size());

  // Delete third marker and portions of second and fourth
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 8, 17);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Verify markers were updated correctly
  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());

  EXPECT_EQ(11u, GetDocument().Markers().Markers()[1]->StartOffset());
  EXPECT_EQ(16u, GetDocument().Markers().Markers()[1]->EndOffset());
}

TEST_F(InputMethodControllerTest, ContentIndependentMarker_Deletions) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(0, 5).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  marker_range = PlainTextRange(5, 10).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  marker_range = PlainTextRange(10, 15).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  marker_range = PlainTextRange(15, 20).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  marker_range = PlainTextRange(20, 25).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kBlack, Color::kBlack);

  EXPECT_EQ(5u, GetDocument().Markers().Markers().size());

  // Delete third marker and portions of second and fourth
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 8, 17);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Verify markers were updated correctly
  EXPECT_EQ(4u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());

  EXPECT_EQ(5u, GetDocument().Markers().Markers()[1]->StartOffset());
  EXPECT_EQ(8u, GetDocument().Markers().Markers()[1]->EndOffset());

  EXPECT_EQ(8u, GetDocument().Markers().Markers()[2]->StartOffset());
  EXPECT_EQ(11u, GetDocument().Markers().Markers()[2]->EndOffset());

  EXPECT_EQ(11u, GetDocument().Markers().Markers()[3]->StartOffset());
  EXPECT_EQ(16u, GetDocument().Markers().Markers()[3]->EndOffset());
}

TEST_F(InputMethodControllerTest,
       ContentDependentMarker_DeleteExactlyOnMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(5, 10).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Delete exactly on the marker
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 5, 10);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_DeleteExactlyOnMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(5, 10).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kBlack);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  // Delete exactly on the marker
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 5, 10);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest, ContentDependentMarker_DeleteMiddleOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(5, 10).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Delete middle of marker
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 6, 9);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  // Verify marker was removed
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_DeleteMiddleOfMarker) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(5, 10).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kBlack);

  // Delete middle of marker
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetCompositionFromExistingText(empty_ime_text_spans, 6, 9);
  Controller().CommitText(String(""), empty_ime_text_spans, 0);

  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(7u, GetDocument().Markers().Markers()[0]->EndOffset());
}

TEST_F(InputMethodControllerTest,
       ContentDependentMarker_InsertInMarkerInterior) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(0, 5).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  marker_range = PlainTextRange(5, 10).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  marker_range = PlainTextRange(10, 15).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  // insert in middle of second marker
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetComposition("", empty_ime_text_spans, 7, 7);
  Controller().CommitText(String("66666"), empty_ime_text_spans, -7);

  EXPECT_EQ(2u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());

  EXPECT_EQ(15u, GetDocument().Markers().Markers()[1]->StartOffset());
  EXPECT_EQ(20u, GetDocument().Markers().Markers()[1]->EndOffset());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_InsertInMarkerInterior) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(0, 5).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kBlack);

  marker_range = PlainTextRange(5, 10).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kBlack);

  marker_range = PlainTextRange(10, 15).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kBlack);

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  // insert in middle of second marker
  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetComposition("", empty_ime_text_spans, 7, 7);
  Controller().CommitText(String("66666"), empty_ime_text_spans, -7);

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());

  EXPECT_EQ(5u, GetDocument().Markers().Markers()[1]->StartOffset());
  EXPECT_EQ(15u, GetDocument().Markers().Markers()[1]->EndOffset());

  EXPECT_EQ(15u, GetDocument().Markers().Markers()[2]->StartOffset());
  EXPECT_EQ(20u, GetDocument().Markers().Markers()[2]->EndOffset());
}

TEST_F(InputMethodControllerTest, ContentDependentMarker_InsertBetweenMarkers) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(0, 5).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  marker_range = PlainTextRange(5, 15).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  marker_range = PlainTextRange(15, 20).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetComposition("", empty_ime_text_spans, 5, 5);
  Controller().CommitText(String("77777"), empty_ime_text_spans, 0);

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());

  EXPECT_EQ(10u, GetDocument().Markers().Markers()[1]->StartOffset());
  EXPECT_EQ(20u, GetDocument().Markers().Markers()[1]->EndOffset());

  EXPECT_EQ(20u, GetDocument().Markers().Markers()[2]->StartOffset());
  EXPECT_EQ(25u, GetDocument().Markers().Markers()[2]->EndOffset());
}

TEST_F(InputMethodControllerTest,
       ContentIndependentMarker_InsertBetweenMarkers) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>1111122222333334444455555</div>",
      "sample");

  EphemeralRange marker_range = PlainTextRange(0, 5).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kBlack);

  marker_range = PlainTextRange(5, 15).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kBlack);

  marker_range = PlainTextRange(15, 20).CreateRange(*div);
  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kBlack, ImeTextSpanThickness::kThin,
      ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent, Color::kBlack);

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  Vector<ImeTextSpan> empty_ime_text_spans;
  Controller().SetComposition("", empty_ime_text_spans, 5, 5);
  Controller().CommitText(String("77777"), empty_ime_text_spans, 0);

  EXPECT_EQ(3u, GetDocument().Markers().Markers().size());

  EXPECT_EQ(0u, GetDocument().Markers().Markers()[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().Markers()[0]->EndOffset());

  EXPECT_EQ(10u, GetDocument().Markers().Markers()[1]->StartOffset());
  EXPECT_EQ(20u, GetDocument().Markers().Markers()[1]->EndOffset());

  EXPECT_EQ(20u, GetDocument().Markers().Markers()[2]->StartOffset());
  EXPECT_EQ(25u, GetDocument().Markers().Markers()[2]->EndOffset());
}

TEST_F(InputMethodControllerTest,
       CommitNotMisspellingSuggestionMarkerWithSpellCheckingDisabled) {
  InsertHTMLElement(
      "<div id='sample' contenteditable spellcheck='false'>text</div>",
      "sample");

  Vector<ImeTextSpan> ime_text_spans;
  // Try to commit a non-misspelling suggestion marker.
  ime_text_spans.push_back(
      ImeTextSpan(ImeTextSpan::Type::kSuggestion, 0, 5, Color::kTransparent,
                  ImeTextSpanThickness::kNone, ImeTextSpanUnderlineStyle::kNone,
                  Color::kTransparent, Color::kTransparent));
  Controller().CommitText("hello", ime_text_spans, 1);

  // The marker should have been added.
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest,
       CommitMisspellingSuggestionMarkerWithSpellCheckingDisabled) {
  InsertHTMLElement(
      "<div id='sample' contenteditable spellcheck='false'>text</div>",
      "sample");

  Vector<ImeTextSpan> ime_text_spans;
  // Try to commit a non-misspelling suggestion marker.
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kMisspellingSuggestion, 0, 5, Color::kTransparent,
      ImeTextSpanThickness::kNone, ImeTextSpanUnderlineStyle::kNone,
      Color::kTransparent, Color::kTransparent));
  Controller().CommitText("hello", ime_text_spans, 1);

  // The marker should not have been added since the div has spell checking
  // disabled.
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

TEST_F(InputMethodControllerTest, RemoveSuggestionMarkerInRangeOnFinish) {
  InsertHTMLElement(
      "<div id='sample' contenteditable spellcheck='true'>text</div>",
      "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kMisspellingSuggestion, 0, 5, Color::kTransparent,
      ImeTextSpanThickness::kNone, ImeTextSpanUnderlineStyle::kNone,
      Color::kTransparent, Color::kTransparent, Color ::kTransparent,
      /* remove_on_finish_composing */ true));

  // Case 1: SetComposition() -> FinishComposingText() removes the suggestion
  // marker when remove_on_finish_composing is true.
  Controller().SetComposition("hello", ime_text_spans, 0, 5);
  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_TRUE(
      Controller().FinishComposingText(InputMethodController::kKeepSelection));

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Case 2: SetComposition() -> CommitText() removes the suggestion marker when
  // remove_on_finish_composing is true.
  Controller().SetComposition("hello", ime_text_spans, 0, 5);
  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());
  ASSERT_TRUE(Controller().CommitText("world", Vector<ImeTextSpan>(), 1));

  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());

  // Case 3: SetComposition() -> SetComposingText() removes the suggestion
  // marker when remove_on_finish_composing is true.
  Controller().SetComposition("hello", ime_text_spans, 0, 5);
  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());
  Controller().SetComposition("helloworld", Vector<ImeTextSpan>(), 0, 10);

  // SetComposing() will add a composition marker.
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());
  EXPECT_EQ(DocumentMarker::MarkerType::kComposition,
            GetDocument().Markers().Markers()[0]->GetType());
}

TEST_F(InputMethodControllerTest, ClearImeTextSpansByType) {
  InsertHTMLElement(
      "<div id='sample' contenteditable spellcheck='true'>hello</div>",
      "sample");
  ImeTextSpan::Type type = ImeTextSpan::Type::kAutocorrect;
  unsigned start = 0;
  unsigned end = 1;
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      type, start, end, Color::kTransparent, ImeTextSpanThickness::kNone,
      ImeTextSpanUnderlineStyle::kNone, Color::kTransparent,
      Color::kTransparent, Color ::kTransparent));

  Controller().AddImeTextSpansToExistingText(ime_text_spans, start, end);
  EXPECT_EQ(1u, GetDocument().Markers().Markers().size());

  Controller().ClearImeTextSpansByType(type, start, end);
  EXPECT_EQ(0u, GetDocument().Markers().Markers().size());
}

// For http://crbug.com/712761
TEST_F(InputMethodControllerTest, TextInputTypeAtBeforeEditable) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().body()->Focus();

  // Set selection before BODY(editable).
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().documentElement(), 0))
          .Build(),
      SetSelectionOptions());

  EXPECT_EQ(kWebTextInputTypeContentEditable, Controller().TextInputType());
}

// http://crbug.com/721666
TEST_F(InputMethodControllerTest, MaxLength) {
  auto* input = To<HTMLInputElement>(
      InsertHTMLElement("<input id='a' maxlength='4'/>", "a"));

  EXPECT_EQ(kWebTextInputTypeText, Controller().TextInputType());

  Controller().SetComposition("abcde", Vector<ImeTextSpan>(), 4, 4);
  EXPECT_EQ("abcde", input->Value());

  Controller().FinishComposingText(InputMethodController::kKeepSelection);
  EXPECT_EQ("abcd", input->Value());
}

TEST_F(InputMethodControllerTest, InputModeOfFocusedElement) {
  InsertHTMLElement("<input id='a' inputmode='decimal'>", "a")->Focus();
  EXPECT_EQ(kWebTextInputModeDecimal, Controller().InputModeOfFocusedElement());

  InsertHTMLElement("<input id='b' inputmode='foo'>", "b")->Focus();
  EXPECT_EQ(kWebTextInputModeDefault, Controller().InputModeOfFocusedElement());
}

TEST_F(InputMethodControllerTest, CompositionUnderlineSpansMultipleNodes) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable><b>t</b>est</div>", "sample");
  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 4, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 0, 4);
  Controller().SetComposition("test", ime_text_spans, 0, 4);

  Node* b = div->firstChild();
  auto* text1 = To<Text>(b->firstChild());
  auto* text2 = To<Text>(b->nextSibling());

  const DocumentMarkerVector& text1_markers =
      GetDocument().Markers().MarkersFor(
          *text1, DocumentMarker::MarkerTypes::Composition());
  EXPECT_EQ(1u, text1_markers.size());
  EXPECT_EQ(0u, text1_markers[0]->StartOffset());
  EXPECT_EQ(1u, text1_markers[0]->EndOffset());

  const DocumentMarkerVector& text2_markers =
      GetDocument().Markers().MarkersFor(
          *text2, DocumentMarker::MarkerTypes::Composition());
  EXPECT_EQ(1u, text2_markers.size());
  EXPECT_EQ(0u, text2_markers[0]->StartOffset());
  EXPECT_EQ(3u, text2_markers[0]->EndOffset());
}

// The following tests are for http://crbug.com/766680.

TEST_F(InputMethodControllerTest, SetCompositionDeletesMarkupBeforeText) {
  Element* div = InsertHTMLElement(
      "<div id='div' contenteditable='true'><img />test</div>", "div");
  // Select the contents of the div element.
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange::RangeOfContents(*div))
          .Build(),
      SetSelectionOptions());

  Controller().SetComposition("t", Vector<ImeTextSpan>(), 0, 1);

  EXPECT_EQ(1u, div->CountChildren());
  auto* text = To<Text>(div->firstChild());
  EXPECT_EQ("t", text->data());
}

TEST_F(InputMethodControllerTest, SetCompositionDeletesMarkupAfterText) {
  Element* div = InsertHTMLElement(
      "<div id='div' contenteditable='true'>test<img /></div>", "div");
  // Select the contents of the div element.
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange::RangeOfContents(*div))
          .Build(),
      SetSelectionOptions());

  Controller().SetComposition("t", Vector<ImeTextSpan>(), 0, 1);

  EXPECT_EQ(1u, div->CountChildren());
  auto* text = To<Text>(div->firstChild());
  EXPECT_EQ("t", text->data());
}

TEST_F(InputMethodControllerTest,
       SetCompositionDeletesMarkupBeforeAndAfterText) {
  Element* div = InsertHTMLElement(
      "<div id='div' contenteditable='true'><img />test<img /></div>", "div");
  // Select the contents of the div element.
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange::RangeOfContents(*div))
          .Build(),
      SetSelectionOptions());

  Controller().SetComposition("t", Vector<ImeTextSpan>(), 0, 1);

  EXPECT_EQ(1u, div->CountChildren());
  auto* text = To<Text>(div->firstChild());
  EXPECT_EQ("t", text->data());
}

TEST_F(InputMethodControllerTest,
       SetCompositionWithPartialGraphemeWithCompositionUnderlineDoesntCrash) {
  InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Vector<ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ImeTextSpan(
      ImeTextSpan::Type::kComposition, 0, 1, Color(255, 0, 0),
      ImeTextSpanThickness::kThin, ImeTextSpanUnderlineStyle::kSolid,
      Color::kTransparent, Color::kTransparent));
  Controller().CommitText(" ", ime_text_spans, 0);
  // Add character U+094D: 'DEVANAGARI SIGN VIRAMA'
  Controller().SetComposition(String::FromUTF8("\xE0\xA5\x8D"), ime_text_spans,
                              1, 1);
}

TEST_F(
    InputMethodControllerTest,
    SetCompositionWithPartialGraphemeWithoutCompositionUnderlineDoesntCrash) {
  InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  Controller().CommitText(" ", Vector<ImeTextSpan>(), 0);
  // Add character U+094D: 'DEVANAGARI SIGN VIRAMA'
  Controller().SetComposition(String::FromUTF8("\xE0\xA5\x8D"),
                              Vector<ImeTextSpan>(), 1, 1);
}

TEST_F(InputMethodControllerTest, SetCompositionContainingNewline) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");
  Controller().SetComposition("Hello", Vector<ImeTextSpan>(), 5, 5);
  Controller().SetComposition("Hello\n", Vector<ImeTextSpan>(), 6, 6);

  EXPECT_EQ("Hello\n\n", PlainText(EphemeralRange(
                             Position(div, PositionAnchorType::kBeforeAnchor),
                             Position(div, PositionAnchorType::kAfterAnchor))));
}

TEST_F(InputMethodControllerTest, SetCompositionTamilVirama) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  // Commit TAMIL LETTER CA (U+0B9A) followed by TAMIL SIGN VIRAMA (U+U0BCD)
  Controller().CommitText(String::FromUTF8("\xE0\xAE\x9A\xE0\xAF\x8D"),
                          Vector<ImeTextSpan>(), 0);

  // Open composition with TAMIL LETTER CA (U+0B9A) followed by
  // TAMIL SIGN VIRAMA (U+U0BCD)
  Controller().SetComposition(String::FromUTF8("\xE0\xAE\x9A\xE0\xAF\x8D"),
                              Vector<ImeTextSpan>(), 2, 2);
  // Remove the TAMIL SIGN VIRAMA from the end of the composition
  Controller().SetComposition(String::FromUTF8("\xE0\xAE\x9A"),
                              Vector<ImeTextSpan>(), 1, 1);

  EXPECT_EQ(1u, div->CountChildren());
  auto* text = To<Text>(div->firstChild());
  EXPECT_EQ("\xE0\xAE\x9A\xE0\xAF\x8D\xE0\xAE\x9A", text->data().Utf8());

  Range* range = GetCompositionRange();
  EXPECT_EQ(2u, range->startOffset());
  EXPECT_EQ(3u, range->endOffset());
}

TEST_F(InputMethodControllerTest,
       CommitTextWithOpenCompositionAndInputEventHandlerChangingText) {
  InsertHTMLElement("<div id='sample' contenteditable>hello</div>", "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.textContent = 'HELLO WORLD';"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 11);"
      "    selection.extend(node.firstChild, 11);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "hello".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 0, 5);

  // Commit text, leaving the cursor at the end of the newly-inserted text.
  // JavaScript will make the text longer by changing it to "HELLO WORLD",
  // while trying to move the selection to the end of the string (where we
  // should leave it).
  Controller().CommitText("HELLO", Vector<ImeTextSpan>(), 0);

  EXPECT_EQ(11, GetFrame()
                    .Selection()
                    .GetSelectionInDOMTree()
                    .Anchor()
                    .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest,
       CommitTextWithoutCompositionAndInputEventHandlerChangingSelection) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>hello world</div>", "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 0);"
      "    selection.extend(node.firstChild, 0);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Select "hello".
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange(Position(div->firstChild(), 0),
                                           Position(div->firstChild(), 5)))
          .Build(),
      SetSelectionOptions());

  // Commit text, leaving the cursor at the end of the newly-inserted text.
  // JavaScript will move the cursor back to the beginning of the
  // "HELLO world", where it should be left.
  Controller().CommitText("HELLO", Vector<ImeTextSpan>(), 0);

  EXPECT_EQ(0, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(
    InputMethodControllerTest,
    SetCompositionToEmptyStringWithOpenCompositionAndInputEventHandlerChangingText) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.textContent = 'HI ';"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 2);"
      "    selection.extend(node.firstChild, 2);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "world".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);

  // Delete the composition range, leaving the cursor in place. JavaScript will
  // change the text and move the cursor after "HI", where it should be left.
  Controller().SetComposition("", Vector<ImeTextSpan>(), 0, 0);

  EXPECT_EQ(2, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest,
       SetCompositionWithOpenCompositionAndInputEventHandlerChangingText) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.textContent = 'HI WORLD';"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 2);"
      "    selection.extend(node.firstChild, 2);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "world".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);

  // Change the composition text, leaving the cursor at the end of the
  // composition. JavaScript will change the text and move the cursor after
  // "HI", where it should be left.
  Controller().SetComposition("WORLD", Vector<ImeTextSpan>(), 5, 5);

  EXPECT_EQ(2, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest,
       SetCompositionWithOpenCompositionAndInputEventHandlerChangingSelection) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 5);"
      "    selection.extend(node.firstChild, 5);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "world".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);

  // Change the composition text, leaving the cursor at the end of the
  // composition. JavaScript should move the cursor after "HELLO", where it
  // should be left.
  Controller().SetComposition("WORLD", Vector<ImeTextSpan>(), 5, 5);

  // The IME cursor update should have been ignored.
  EXPECT_EQ(5, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest,
       SetCompositionToEmptyStringAndInputEventHandlerChangingSelection) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 5);"
      "    selection.extend(node.firstChild, 5);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "world".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);

  // Change the composition text to the empty string (so we end up with
  // "hello ") and move the cursor to before "hello". JavaScript will change
  // the text and move the cursor after "hello", where it should be left.
  Controller().SetComposition("", Vector<ImeTextSpan>(), -6, -6);

  EXPECT_EQ(5, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest,
       SetCompositionDeleteSelectionAndInputEventHandlerChangingSelection) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>hello world</div>", "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 5);"
      "    selection.extend(node.firstChild, 5);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Select "world".
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange(Position(div->firstChild(), 6),
                                           Position(div->firstChild(), 11)))
          .Build(),
      SetSelectionOptions());

  // Call SetComposition() passing the empty string to delete the selection
  // (so we end up with "hello ") and move the cursor to before "hello".
  // JavaScript will change the text and move the cursor after "hello", where
  // it should be left.
  Controller().SetComposition("", Vector<ImeTextSpan>(), -6, -6);

  EXPECT_EQ(5, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest,
       DeleteSelectionAndBeforeInputEventHandlerChangingStyle) {
  Element* div = InsertHTMLElement(
      "<div id='sample' contenteditable>hello world</div>", "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('beforeinput', "
      "  event => {"
      "    event.currentTarget.style.transform = 'rotate(7deg)';"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Select "world".
  GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange(Position(div->firstChild(), 6),
                                           Position(div->firstChild(), 11)))
          .Build(),
      SetSelectionOptions());

  // Call DeleteSelection() will fire beforeinput event before deleting
  // selection. The beforeinput event handler dirties the layout. We should
  // update layout again before calling |TypingCommand::DeleteSelection()| to
  // avoid crash.
  EXPECT_EQ(true, Controller().DeleteSelection());
}

TEST_F(InputMethodControllerTest,
       CommitTextWithOpenCompositionAndCompositionEndEventHandlerChangingText) {
  InsertHTMLElement("<div id='sample' contenteditable>hello</div>", "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.textContent = 'HELLO WORLD';"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 11);"
      "    selection.extend(node.firstChild, 11);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "hello".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 0, 5);

  // Commit text, leaving the cursor at the end of the newly-inserted text.
  // JavaScript will make the text longer by changing it to "HELLO WORLD",
  // while trying to move the selection to the end of the string (where we
  // should leave it).
  Controller().CommitText("HELLO", Vector<ImeTextSpan>(), 0);

  EXPECT_EQ(11, GetFrame()
                    .Selection()
                    .GetSelectionInDOMTree()
                    .Anchor()
                    .ComputeOffsetInContainerNode());
}

TEST_F(
    InputMethodControllerTest,
    SetCompositionToEmptyStringWithOpenCompositionAndCompositionEndEventHandlerChangingText) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.textContent = 'HI ';"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 2);"
      "    selection.extend(node.firstChild, 2);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "world".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);

  // Delete the composition range, leaving the cursor in place. JavaScript will
  // change the text and move the cursor after "HI", where it should be left.
  Controller().SetComposition("", Vector<ImeTextSpan>(), 0, 0);

  EXPECT_EQ(2, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(
    InputMethodControllerTest,
    SetCompositionToEmptyStringAndCompositionEndEventHandlerChangingSelection) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 5);"
      "    selection.extend(node.firstChild, 5);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "world".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);

  // Change the composition text to the empty string (so we end up with
  // "hello ") and move the cursor to before "hello". JavaScript will change
  // the text and move the cursor after "hello", where it should be left.
  Controller().SetComposition("", Vector<ImeTextSpan>(), -6, -6);

  EXPECT_EQ(5, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest,
       FinishComposingTextDoNotKeepSelectionAndCompositionEndEventHandler) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 5);"
      "    selection.extend(node.firstChild, 5);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "world".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);

  // JavaScript will change the text and move the cursor after "hello", where
  // it should be left.
  Controller().FinishComposingText(InputMethodController::kKeepSelection);

  EXPECT_EQ(5, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest,
       FinishComposingTextKeepSelectionAndCompositionEndEventHandler) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    const selection = getSelection();"
      "    selection.collapse(node.firstChild, 5);"
      "    selection.extend(node.firstChild, 5);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Open composition on "world".
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);

  // JavaScript will change the text and move the cursor after "hello", where
  // it should be left.
  Controller().FinishComposingText(InputMethodController::kDoNotKeepSelection);

  EXPECT_EQ(5, GetFrame()
                   .Selection()
                   .GetSelectionInDOMTree()
                   .Anchor()
                   .ComputeOffsetInContainerNode());
}

TEST_F(
    InputMethodControllerTest,
    SetCompositionFromExistingTextAndCompositionStartEventHandlerChangingStyle) {
  InsertHTMLElement("<div id='sample' contenteditable>hello world</div>",
                    "sample");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('compositionstart', "
      "  event => {"
      "    event.currentTarget.style.transform = 'rotate(7deg)';"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  // Call SetCompositionFromExistingText() will fire compositionstart event. The
  // compositionstart event handler dirties the layout. We should update layout
  // again before getting visible selection to avoid crash.
  Controller().SetCompositionFromExistingText(Vector<ImeTextSpan>(), 6, 11);
}

TEST_F(InputMethodControllerTest,
       FinishComposingTextTooLongKeepSelectionAndInputEventHandler) {
  auto* input = To<HTMLInputElement>(
      InsertHTMLElement("<input id='sample' maxlength='2'>", "sample"));

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.setSelectionRange(1, 1);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  input->Focus();

  // Open a composition that's too long for the <input> element..
  Controller().SetComposition("hello", Vector<ImeTextSpan>(), 0, 0);

  // Close out the composition, triggering the input event handler.
  Controller().FinishComposingText(InputMethodController::kKeepSelection);
  EXPECT_EQ("he", input->Value());

  // Verify that the input handler was able to properly move the selection.
  EXPECT_EQ(1u, input->selectionStart());
  EXPECT_EQ(1u, input->selectionEnd());
}

TEST_F(InputMethodControllerTest,
       FinishComposingTextTooLongDoNotKeepSelectionAndInputEventHandler) {
  auto* input = To<HTMLInputElement>(
      InsertHTMLElement("<input id='sample' maxlength='2'>", "sample"));

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('input', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.setSelectionRange(1, 1);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  input->Focus();

  // Open a composition that's too long for the <input> element..
  Controller().SetComposition("hello", Vector<ImeTextSpan>(), 0, 0);

  // Close out the composition, triggering the input event handler.
  Controller().FinishComposingText(InputMethodController::kDoNotKeepSelection);
  EXPECT_EQ("he", input->Value());

  // Verify that the input handler was able to properly move the selection.
  EXPECT_EQ(1u, input->selectionStart());
  EXPECT_EQ(1u, input->selectionEnd());
}

TEST_F(InputMethodControllerTest,
       FinishComposingTextTooLongKeepSelectionAndCompositionEndEventHandler) {
  auto* input = To<HTMLInputElement>(
      InsertHTMLElement("<input id='sample' maxlength='2'>", "sample"));

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.setSelectionRange(1, 1);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  input->Focus();

  // Open a composition that's too long for the <input> element..
  Controller().SetComposition("hello", Vector<ImeTextSpan>(), 0, 0);

  // Close out the composition, triggering the compositionend event handler.
  Controller().FinishComposingText(InputMethodController::kKeepSelection);
  EXPECT_EQ("he", input->Value());

  // Verify that the compositionend handler was able to properly move the
  // selection.
  EXPECT_EQ(1u, input->selectionStart());
  EXPECT_EQ(1u, input->selectionEnd());
}

TEST_F(
    InputMethodControllerTest,
    FinishComposingTextTooLongDoNotKeepSelectionAndCompositionEndEventHandler) {
  auto* input = To<HTMLInputElement>(
      InsertHTMLElement("<input id='sample' maxlength='2'>", "sample"));

  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('compositionend', "
      "  event => {"
      "    const node = event.currentTarget;"
      "    node.setSelectionRange(1, 1);"
      "});");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  input->Focus();

  // Open a composition that's too long for the <input> element..
  Controller().SetComposition("hello", Vector<ImeTextSpan>(), 0, 0);

  // Close out the composition, triggering the compositionend event handler.
  Controller().FinishComposingText(InputMethodController::kDoNotKeepSelection);
  EXPECT_EQ("he", input->Value());

  // Verify that the compositionend handler was able to properly move the
  // selection.
  EXPECT_EQ(1u, input->selectionStart());
  EXPECT_EQ(1u, input->selectionEnd());
}

TEST_F(InputMethodControllerTest, AutocapitalizeTextInputFlags) {
  // This test assumes that the behavior tested in
  // web_tests/fast/forms/autocapitalize.html works properly and tests the
  // following:
  // - The autocapitalize IDL states map properly to WebTextInputFlags for
  //   <input> elements, <textarea> elements, and editable regions
  // - We ignore the value of the IDL attribute for password/email/URL inputs
  //   and always send None for this case.
  Vector<std::pair<String, int>> element_and_expected_flags_pairs = {
      {"<input type='text'>", kWebTextInputFlagAutocapitalizeSentences},
      {"<input type='text' autocapitalize='none'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='text' autocapitalize='characters'>",
       kWebTextInputFlagAutocapitalizeCharacters},
      {"<input type='text' autocapitalize='sentences'>",
       kWebTextInputFlagAutocapitalizeSentences},
      {"<input type='text' autocapitalize='words'>",
       kWebTextInputFlagAutocapitalizeWords},

      {"<input type='search'>", kWebTextInputFlagAutocapitalizeSentences},
      {"<input type='search' autocapitalize='none'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='search' autocapitalize='characters'>",
       kWebTextInputFlagAutocapitalizeCharacters},
      {"<input type='search' autocapitalize='sentences'>",
       kWebTextInputFlagAutocapitalizeSentences},
      {"<input type='search' autocapitalize='words'>",
       kWebTextInputFlagAutocapitalizeWords},

      {"<input type='email'>", kWebTextInputFlagAutocapitalizeNone},
      {"<input type='email' autocapitalize='none'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='email' autocapitalize='characters'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='email' autocapitalize='sentences'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='email' autocapitalize='words'>",
       kWebTextInputFlagAutocapitalizeNone},

      {"<input type='url'>", kWebTextInputFlagAutocapitalizeNone},
      {"<input type='url' autocapitalize='none'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='url' autocapitalize='characters'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='url' autocapitalize='sentences'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='url' autocapitalize='words'>",
       kWebTextInputFlagAutocapitalizeNone},

      {"<input type='password'>", kWebTextInputFlagAutocapitalizeNone},
      {"<input type='password' autocapitalize='none'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='password' autocapitalize='characters'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='password' autocapitalize='sentences'>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<input type='password' autocapitalize='words'>",
       kWebTextInputFlagAutocapitalizeNone},

      {"<textarea></textarea>", kWebTextInputFlagAutocapitalizeSentences},
      {"<textarea autocapitalize='none'></textarea>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<textarea autocapitalize='characters'></textarea>",
       kWebTextInputFlagAutocapitalizeCharacters},
      {"<textarea autocapitalize='sentences'></textarea>",
       kWebTextInputFlagAutocapitalizeSentences},
      {"<textarea autocapitalize='words'></textarea>",
       kWebTextInputFlagAutocapitalizeWords},

      {"<div contenteditable></div>", kWebTextInputFlagAutocapitalizeSentences},
      {"<div contenteditable autocapitalize='none'></div>",
       kWebTextInputFlagAutocapitalizeNone},
      {"<div contenteditable autocapitalize='characters'></div>",
       kWebTextInputFlagAutocapitalizeCharacters},
      {"<div contenteditable autocapitalize='sentences'></div>",
       kWebTextInputFlagAutocapitalizeSentences},
      {"<div contenteditable autocapitalize='words'></div>",
       kWebTextInputFlagAutocapitalizeWords},
  };

  const int autocapitalize_mask = kWebTextInputFlagAutocapitalizeNone |
                                  kWebTextInputFlagAutocapitalizeCharacters |
                                  kWebTextInputFlagAutocapitalizeWords |
                                  kWebTextInputFlagAutocapitalizeSentences;

  for (const std::pair<String, int>& element_and_expected_flags_pair :
       element_and_expected_flags_pairs) {
    const String& element = element_and_expected_flags_pair.first;
    const int expected_flags = element_and_expected_flags_pair.second;

    GetDocument().write(element);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    To<Element>(GetDocument().body()->lastChild())->Focus();

    EXPECT_EQ(expected_flags,
              Controller().TextInputInfo().flags & autocapitalize_mask);
  }
}

TEST_F(InputMethodControllerTest, VerticalTextInputFlags) {
  Vector<std::pair<String, int>> element_html_and_expected_flags = {
      {"<div contenteditable='true'></div>", 0},
      {"<div contenteditable='true' style='writing-mode:vertical-rl;'></div>",
       kWebTextInputFlagVertical},
      {"<div contenteditable='true' style='writing-mode:vertical-lr;'></div>",
       kWebTextInputFlagVertical},
  };

  for (const std::pair<String, int>& html_and_flags :
       element_html_and_expected_flags) {
    const String& element_html = html_and_flags.first;
    const int expected_flags = html_and_flags.second;

    GetDocument().write(element_html);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    To<Element>(GetDocument().body()->lastChild())->Focus();

    EXPECT_EQ(expected_flags,
              Controller().TextInputInfo().flags & kWebTextInputFlagVertical);
  }
}

TEST_F(InputMethodControllerTest, ExecCommandDuringComposition) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  // Open a composition.
  Controller().SetComposition(String::FromUTF8("hello"), Vector<ImeTextSpan>(),
                              5, 5);
  // Turn on bold formatting.
  GetDocument().execCommand("bold", false, "", ASSERT_NO_EXCEPTION);

  // Extend the composition with some more text.
  Controller().SetComposition(String::FromUTF8("helloworld"),
                              Vector<ImeTextSpan>(), 10, 10);

  // "world" should be bold.
  EXPECT_EQ("hello<b>world</b>", div->innerHTML());
}

TEST_F(InputMethodControllerTest, SetCompositionAfterNonEditableElement) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody("<div id='sample' contenteditable='true'>"
                             "<span contenteditable='false'>a</span>|b</div>"),
      SetSelectionOptions());
  Element* const div = GetDocument().getElementById(AtomicString("sample"));
  div->Focus();

  // Open a composition and insert some text.
  Controller().SetComposition(String::FromUTF8("c"), Vector<ImeTextSpan>(), 1,
                              1);

  // Add some more text to the composition.
  Controller().SetComposition(String::FromUTF8("cd"), Vector<ImeTextSpan>(), 2,
                              2);

  EXPECT_EQ(
      "<div contenteditable=\"true\" id=\"sample\">"
      "<span contenteditable=\"false\">a</span>^cd|b</div>",
      GetSelectionTextFromBody(
          SelectionInDOMTree::Builder()
              .SetBaseAndExtent(Controller().CompositionEphemeralRange())
              .Build()));
}

TEST_F(InputMethodControllerTest, SetCompositionInTableCell) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody(
          "<table id='sample' contenteditable><tr><td>a</td><td "
          "id='td2'>|</td></tr></table>"),
      SetSelectionOptions());
  Element* const table = GetDocument().getElementById(AtomicString("sample"));
  table->Focus();

  Controller().SetComposition(String::FromUTF8("c"), Vector<ImeTextSpan>(), 1,
                              1);

  Element* const td2 = GetDocument().getElementById(AtomicString("td2"));
  const Node* const text_node = td2->firstChild();

  Range* range = GetCompositionRange();
  EXPECT_EQ(text_node, range->startContainer());
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(text_node, range->endContainer());
  EXPECT_EQ(1u, range->endOffset());
}

TEST_F(InputMethodControllerTest, SetCompositionInMyanmar) {
  Element* div =
      InsertHTMLElement("<div id='sample' contenteditable></div>", "sample");

  // Add character U+200C: 'kZeroWidthNonJoinerCharacter' and Myanmar vowel
  Controller().SetComposition(String::FromUTF8("\xE2\x80\x8C\xE1\x80\xB1"),
                              Vector<ImeTextSpan>(), 0, 0);

  EXPECT_EQ(1u, div->CountChildren());
  EXPECT_EQ(String::FromUTF8("\xE2\x80\x8C\xE1\x80\xB1"), div->innerHTML());

  Range* range = GetCompositionRange();
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(2u, range->endOffset());
  Controller().CommitText(String::FromUTF8("\xE2\x80\x8C\xE1\x80\xB1"),
                          Vector<ImeTextSpan>(), 1);
  EXPECT_EQ(String::FromUTF8("\xE2\x80\x8C\xE1\x80\xB1"), div->innerHTML());

  // Add character U+200C: 'kZeroWidthNonJoinerCharacter' and Myanmar vowel
  Controller().SetComposition(String::FromUTF8("\xE2\x80\x8C\xE1\x80\xB1"),
                              Vector<ImeTextSpan>(), 2, 2);
  Controller().CommitText(String::FromUTF8("\xE2\x80\x8C\xE1\x80\xB1"),
                          Vector<ImeTextSpan>(), 1);
  EXPECT_EQ(
      String::FromUTF8("\xE2\x80\x8C\xE1\x80\xB1\xE2\x80\x8C\xE1\x80\xB1"),
      div->innerHTML());
}

TEST_F(InputMethodControllerTest, VirtualKeyboardPolicyOfFocusedElement) {
  EXPECT_EQ(ui::mojom::VirtualKeyboardPolicy::AUTO,
            Controller().VirtualKeyboardPolicyOfFocusedElement());
  InsertHTMLElement("<input id='a' virtualkeyboardpolicy='manual'>", "a")
      ->Focus();
  EXPECT_EQ(ui::mojom::VirtualKeyboardPolicy::MANUAL,
            Controller().VirtualKeyboardPolicyOfFocusedElement());
}

TEST_F(InputMethodControllerTest, SetCompositionInTibetan) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody("<div id='sample' contenteditable>|</div>"),
      SetSelectionOptions());
  Element* const div = GetDocument().getElementById(AtomicString("sample"));
  div->Focus();

  Vector<ImeTextSpan> ime_text_spans;
  Controller().SetComposition(String(Vector<UChar>{0xF56}), ime_text_spans, 1,
                              1);
  EXPECT_EQ("<div contenteditable id=\"sample\">\u0F56|</div>",
            GetSelectionTextFromBody());

  Controller().CommitText(String(Vector<UChar>{0xF56}), ime_text_spans, 0);
  EXPECT_EQ("<div contenteditable id=\"sample\">\u0F56|</div>",
            GetSelectionTextFromBody());

  Controller().SetComposition(String(Vector<UChar>{0xFB7}), ime_text_spans, 1,
                              1);
  EXPECT_EQ("<div contenteditable id=\"sample\">\u0F56\u0FB7|</div>",
            GetSelectionTextFromBody());

  // Attempt to replace part of grapheme cluster "\u0FB7" in composition
  Controller().CommitText(String(Vector<UChar>{0xFB7}), ime_text_spans, 0);
  EXPECT_EQ("<div contenteditable id=\"sample\">\u0F56\u0FB7|</div>",
            GetSelectionTextFromBody());

  Controller().SetComposition(String(Vector<UChar>{0xF74}), ime_text_spans, 1,
                              1);
  EXPECT_EQ("<div contenteditable id=\"sample\">\u0F56\u0FB7\u0F74|</div>",
            GetSelectionTextFromBody());
}

TEST_F(InputMethodControllerTest, SetCompositionInDevanagari) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody("<div id='sample' contenteditable>\u0958|</div>"),
      SetSelectionOptions());
  Element* const div = GetDocument().getElementById(AtomicString("sample"));
  div->Focus();

  Vector<ImeTextSpan> ime_text_spans;
  Controller().SetComposition(String(Vector<UChar>{0x94D}), ime_text_spans, 1,
                              1);
  EXPECT_EQ("<div contenteditable id=\"sample\">\u0958\u094D|</div>",
            GetSelectionTextFromBody());

  Controller().CommitText(String(Vector<UChar>{0x94D, 0x930}), ime_text_spans,
                          0);
  EXPECT_EQ("<div contenteditable id=\"sample\">\u0958\u094D\u0930|</div>",
            GetSelectionTextFromBody());
}

TEST_F(InputMethodControllerTest, SetCompositionTamil) {
  GetFrame().Selection().SetSelection(
      SetSelectionTextToBody("<div id='sample' contenteditable>|</div>"),
      SetSelectionOptions());
  Element* const div = GetDocument().getElementById(AtomicString("sample"));
  div->Focus();

  Vector<ImeTextSpan> ime_text_spans;
  // Note: region starts out with space.
  Controller().CommitText(String(Vector<UChar>{0xA0}), ime_text_spans, 0);
  // Add character U+0BB5: 'TAMIL LETTER VA'
  Controller().SetComposition(String(Vector<UChar>{0xBB5}), ime_text_spans, 0,
                              0);
  // Add character U+0BC7: 'TAMIL VOWEL SIGN EE'
  Controller().CommitText(String(Vector<UChar>{0xBB5, 0xBC7}), ime_text_spans,
                          1);
  EXPECT_EQ("<div contenteditable id=\"sample\">\u00A0\u0BB5\u0BC7|</div>",
            GetSelectionTextFromBody());
}

TEST_F(InputMethodControllerTest, EditContextCanvasHasEditableType) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  Element* noneditable_canvas = InsertHTMLElement(
      "<canvas id='noneditable-canvas'></canvas>", "noneditable-canvas");
  Element* editable_canvas = InsertHTMLElement(
      "<canvas id='editable-canvas'></canvas>", "editable-canvas");
  Element* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setInnerHTML(
      "document.getElementById('editable-canvas').editContext = new "
      "EditContext()");
  GetDocument().body()->AppendChild(script);
  UpdateAllLifecyclePhasesForTest();

  noneditable_canvas->Focus();
  EXPECT_EQ(kWebTextInputTypeNone, Controller().TextInputType());

  editable_canvas->Focus();
  EXPECT_EQ(kWebTextInputTypeContentEditable, Controller().TextInputType());
}
}  // namespace blink
