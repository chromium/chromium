// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_paragraph_element.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// The implementation of Element#innerText algorithm[1].
// [1]
// https://html.spec.whatwg.org/multipage/dom.html#the-innertext-idl-attribute
class ElementInnerTextCollector final {
 public:
  ElementInnerTextCollector() = default;

  String RunOn(const Element& element);

 private:
  // Result characters of innerText collection steps.
  class Result final {
   public:
    Result() = default;

    void EmitBeginBlock();
    void EmitChar16(UChar code_point);
    void EmitCollapsibleSpace();
    void EmitEndBlock();
    void EmitNewline();
    void EmitRequiredLineBreak(int count);
    void EmitTab();
    void EmitText(const StringView& text);
    String Finish();
    void FlushCollapsibleSpace();

    void SetShouldCollapseWhitespace(bool value) {
      should_collapse_white_space_ = value;
    }

   private:
    void FlushRequiredLineBreak();

    StringBuilder builder_;
    int required_line_break_count_ = 0;

    // |should_collapse_white_space_| is used for collapsing white spaces around
    // block, e.g. leading white space at start of block and leading white
    // spaces after inline-block.
    bool should_collapse_white_space_ = false;
    bool has_collapsible_space_ = false;

    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  // Minimal CSS text box representation for collecting character.
  struct TextBox {
    StringView text;
    // An offset in |LayoutText::GetText()| or |NGInlineItemsData.text_content|.
    unsigned start = 0;

    TextBox(StringView passed_text, unsigned passed_start)
        : text(passed_text), start(passed_start) {
      DCHECK_GT(text.length(), 0u);
    }
  };

  static bool EndsWithWhiteSpace(const InlineTextBox& text_box);
  static bool EndsWithWhiteSpace(const LayoutText& layout_text);
  static bool EndsWithWhiteSpace(const NGPhysicalTextFragment& fragment);
  static bool HasDisplayContentsStyle(const Node& node);
  static bool IsAfterWhiteSpace(const InlineTextBox& text_box);
  static bool IsAfterWhiteSpace(const LayoutText& layout_text);
  static bool IsAfterWhiteSpace(const NGPhysicalTextFragment& fragment);
  static bool IsBeingRendered(const Node& node);
  static bool IsCollapsibleSpace(UChar code_point);
  // Returns true if used value of "display" is block-level.
  static bool IsDisplayBlockLevel(const Node&);
  static LayoutObject* PreviousLeafOf(const LayoutObject& layout_object);
  static bool ShouldEmitNewlineForTableRow(const LayoutTableRow& table_row);
  static bool StartsWithWhiteSpace(const LayoutText& layout_text);

  void ProcessChildren(const Node& node);
  void ProcessChildrenWithRequiredLineBreaks(const Node& node,
                                             int required_line_break_count);
  void ProcessLayoutText(const LayoutText& layout_text);
  void ProcessLayoutTextEmpty(const LayoutText& layout_text);
  void ProcessLayoutTextForNG(const NGPaintFragment::FragmentRange& fragments);
  void ProcessNode(const Node& node);
  void ProcessOptionElement(const HTMLOptionElement& element);
  void ProcessSelectElement(const HTMLSelectElement& element);
  void ProcessText(StringView text, EWhiteSpace white_space);
  void ProcessTextBoxes(const LayoutText& layout_text,
                        const Vector<TextBox>& text_boxes);
  void ProcessTextNode(const Text& node);

  // Result character buffer.
  Result result_;

  DISALLOW_COPY_AND_ASSIGN(ElementInnerTextCollector);
};

String ElementInnerTextCollector::RunOn(const Element& element) {
  DCHECK(!element.InActiveDocument() || !NeedsLayoutTreeUpdate(element));

  // 1. If this element is not being rendered, or if the user agent is a non-CSS
  // user agent, then return the same value as the textContent IDL attribute on
  // this element.
  // Note: To pass WPT test, case we don't use |textContent| for
  // "display:content". See [1] for discussion about "display:contents" and
  // "being rendered".
  // [1] https://github.com/whatwg/html/issues/1837
  if (!IsBeingRendered(element) && !HasDisplayContentsStyle(element)) {
    const bool convert_brs_to_newlines = false;
    return element.textContent(convert_brs_to_newlines);
  }

  // 2. Let results be the list resulting in running the inner text collection
  // steps with this element. Each item in results will either be a JavaScript
  // string or a positive integer (a required line break count).
  ProcessNode(element);
  return result_.Finish();
}

// static
bool ElementInnerTextCollector::EndsWithWhiteSpace(
    const InlineTextBox& text_box) {
  const unsigned length = text_box.Len();
  if (length == 0)
    return false;
  const String text = text_box.GetLineLayoutItem().GetText();
  return IsCollapsibleSpace(text[text_box.Start() + length - 1]);
}

// static
bool ElementInnerTextCollector::EndsWithWhiteSpace(
    const LayoutText& layout_text) {
  const unsigned length = layout_text.TextLength();
  return length > 0 && layout_text.ContainsOnlyWhitespace(length - 1, 1);
}

// static
bool ElementInnerTextCollector::EndsWithWhiteSpace(
    const NGPhysicalTextFragment& fragment) {
  return IsCollapsibleSpace(fragment.Text()[fragment.Length() - 1]);
}

// static
bool ElementInnerTextCollector::HasDisplayContentsStyle(const Node& node) {
  return node.IsElementNode() && ToElement(node).HasDisplayContentsStyle();
}

// An element is *being rendered* if it has any associated CSS layout boxes,
// SVG layout boxes, or some equivalent in other styling languages.
// Note: Just being off-screen does not mean the element is not being rendered.
// The presence of the "hidden" attribute normally means the element is not
// being rendered, though this might be overridden by the style sheets.
// From https://html.spec.whatwg.org/multipage/rendering.html#being-rendered
// static
bool ElementInnerTextCollector::IsBeingRendered(const Node& node) {
  return node.GetLayoutObject();
}

// static
bool ElementInnerTextCollector::IsAfterWhiteSpace(
    const InlineTextBox& text_box) {
  const unsigned start = text_box.Start();
  if (start == 0)
    return false;
  const String text = text_box.GetLineLayoutItem().GetText();
  return IsCollapsibleSpace(text[start - 1]);
}

// static
bool ElementInnerTextCollector::IsAfterWhiteSpace(
    const LayoutText& layout_text) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    const auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_text);
    if (!fragments.IsEmpty() &&
        fragments.IsInLayoutNGInlineFormattingContext()) {
      NGPaintFragmentTraversalContext previous =
          NGPaintFragmentTraversal::PreviousInlineLeafOfIgnoringLineBreak(
              NGPaintFragmentTraversalContext::Create(&fragments.front()));
      if (previous.IsNull())
        return false;
      const NGPhysicalFragment& previous_fragment =
          previous.GetFragment()->PhysicalFragment();
      if (!previous_fragment.IsText())
        return false;
      return EndsWithWhiteSpace(ToNGPhysicalTextFragment(previous_fragment));
    }
  }
  if (InlineTextBox* text_box = layout_text.FirstTextBox()) {
    const InlineBox* previous = text_box->PrevLeafChild();
    if (!previous || !previous->IsInlineTextBox())
      return false;
    return EndsWithWhiteSpace(ToInlineTextBox(*previous));
  }
  const LayoutObject* previous_leaf = PreviousLeafOf(layout_text);
  if (!previous_leaf || !previous_leaf->IsText())
    return false;
  const LayoutText& previous_text = ToLayoutText(*previous_leaf);
  const unsigned length = previous_text.TextLength();
  if (length == 0)
    return false;
  return previous_text.ContainsOnlyWhitespace(length - 1, 1);
}

bool ElementInnerTextCollector::IsAfterWhiteSpace(
    const NGPhysicalTextFragment& text_box) {
  const unsigned start = text_box.StartOffset();
  if (start == 0)
    return false;
  const String text = text_box.TextContent();
  return IsCollapsibleSpace(text[start - 1]);
}

// See https://drafts.csswg.org/css-text-3/#white-space-phase-2
bool ElementInnerTextCollector::IsCollapsibleSpace(UChar code_point) {
  return code_point == kSpaceCharacter || code_point == kNewlineCharacter ||
         code_point == kTabulationCharacter ||
         code_point == kCarriageReturnCharacter;
}

// static
bool ElementInnerTextCollector::IsDisplayBlockLevel(const Node& node) {
  const LayoutObject* const layout_object = node.GetLayoutObject();
  if (!layout_object)
    return false;
  if (!layout_object->IsLayoutBlock()) {
    if (layout_object->IsTableSection()) {
      // Note: |LayoutTableSeleciton::IsInline()| returns false, but it is not
      // block-level.
      return false;
    }
    // Note: Block-level replaced elements, e.g. <img style=display:block>,
    // reach here. Unlike |LayoutBlockFlow::AddChild()|, innerText considers
    // floats and absolutely-positioned elements as block-level node.
    return !layout_object->IsInline();
  }
  // TODO(crbug.com/567964): Due by the issue, |IsAtomicInlineLevel()| is always
  // true for replaced elements event if it has display:block, once it is fixed
  // we should check at first.
  if (layout_object->IsAtomicInlineLevel())
    return false;
  if (layout_object->IsRubyText()) {
    // RT isn't consider as block-level.
    // e.g. <ruby>abc<rt>def</rt>.innerText == "abcdef"
    return false;
  }
  // Note: CAPTION is associated to |LayoutNGTableCaption| in LayoutNG or
  // |LayoutBlockFlow| in legacy layout.
  return true;
}

// static
LayoutObject* ElementInnerTextCollector::PreviousLeafOf(
    const LayoutObject& layout_object) {
  LayoutObject* parent = layout_object.Parent();
  for (LayoutObject* runner = layout_object.PreviousInPreOrder(); runner;
       runner = runner->PreviousInPreOrder()) {
    if (runner != parent)
      return runner;
    parent = runner->Parent();
  }
  return nullptr;
}

// static
bool ElementInnerTextCollector::ShouldEmitNewlineForTableRow(
    const LayoutTableRow& table_row) {
  const LayoutTable* const table = table_row.Table();
  if (!table)
    return false;
  if (table_row.NextRow())
    return true;
  // For TABLE contains TBODY, TFOOTER, THEAD.
  const LayoutTableSection* const table_section = table_row.Section();
  if (!table_section)
    return false;
  // See |LayoutTable::SectionAbove()| and |SectionBelow()| for traversing
  // |LayoutTableSection|.
  for (LayoutObject* runner = table_section->NextSibling(); runner;
       runner = runner->NextSibling()) {
    if (!runner->IsTableSection())
      continue;
    if (ToLayoutTableSection(*runner).NumRows() > 0)
      return true;
  }
  // No table row after |node|.
  return false;
}

// static
bool ElementInnerTextCollector::StartsWithWhiteSpace(
    const LayoutText& layout_text) {
  const unsigned length = layout_text.TextLength();
  return length > 0 && layout_text.ContainsOnlyWhitespace(0, 1);
}

void ElementInnerTextCollector::ProcessChildren(const Node& container) {
  for (const Node& node : NodeTraversal::ChildrenOf(container))
    ProcessNode(node);
}

void ElementInnerTextCollector::ProcessChildrenWithRequiredLineBreaks(
    const Node& node,
    int required_line_break_count) {
  DCHECK_GE(required_line_break_count, 0);
  DCHECK_LE(required_line_break_count, 2);
  result_.EmitBeginBlock();
  result_.EmitRequiredLineBreak(required_line_break_count);
  ProcessChildren(node);
  result_.EmitRequiredLineBreak(required_line_break_count);
  result_.EmitEndBlock();
}

void ElementInnerTextCollector::ProcessLayoutText(
    const LayoutText& layout_text) {
  if (layout_text.TextLength() == 0)
    return;
  if (layout_text.Style()->Visibility() != EVisibility::kVisible) {
    // TODO(editing-dev): Once we make ::first-letter don't apply "visibility",
    // we should get rid of this if-statement. http://crbug.com/866744
    return;
  }
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    const auto fragments = NGPaintFragment::InlineFragmentsFor(&layout_text);
    if (!fragments.IsEmpty() &&
        fragments.IsInLayoutNGInlineFormattingContext()) {
      ProcessLayoutTextForNG(fragments);
      return;
    }
  }

  if (!layout_text.FirstTextBox()) {
    if (!layout_text.ContainsOnlyWhitespace(0, layout_text.TextLength()))
      return;
    if (IsAfterWhiteSpace(layout_text))
      return;
    // <div style="width:0">abc<span> <span>def</span></div> reaches here for
    // a space between SPAN.
    result_.EmitCollapsibleSpace();
    return;
  }

  // TODO(editing-dev): We should handle "text-transform" in "::first-line".
  // In legacy layout, |InlineTextBox| holds original text and text box
  // paint does text transform.
  const String text = layout_text.GetText();
  const bool collapse_white_space = layout_text.Style()->CollapseWhiteSpace();
  bool may_have_leading_space = collapse_white_space &&
                                StartsWithWhiteSpace(layout_text) &&
                                !IsAfterWhiteSpace(layout_text);
  Vector<TextBox> text_boxes;
  for (InlineTextBox* text_box : layout_text.TextBoxes()) {
    const unsigned start =
        may_have_leading_space && IsAfterWhiteSpace(*text_box)
            ? text_box->Start() - 1
            : text_box->Start();
    const unsigned end = text_box->Start() + text_box->Len();
    may_have_leading_space =
        collapse_white_space && !IsCollapsibleSpace(text[end - 1]);
    text_boxes.emplace_back(StringView(text, start, end - start), start);
  }
  ProcessTextBoxes(layout_text, text_boxes);
  if (!collapse_white_space || !EndsWithWhiteSpace(layout_text))
    return;
  result_.EmitCollapsibleSpace();
}

void ElementInnerTextCollector::ProcessLayoutTextForNG(
    const NGPaintFragment::FragmentRange& paint_fragments) {
  DCHECK(!paint_fragments.IsEmpty());
  const LayoutText& layout_text =
      ToLayoutText(*paint_fragments.front().GetLayoutObject());
  const bool collapse_white_space = layout_text.Style()->CollapseWhiteSpace();
  bool may_have_leading_space = collapse_white_space &&
                                StartsWithWhiteSpace(layout_text) &&
                                !IsAfterWhiteSpace(layout_text);
  // TODO(editing-dev): We should include overflow text to result of CSS
  // "text-overflow". See http://crbug.com/873957
  Vector<TextBox> text_boxes;
  const StringImpl* last_text_content = nullptr;
  for (const NGPaintFragment* paint_fragment : paint_fragments) {
    const NGPhysicalTextFragment& text_fragment =
        ToNGPhysicalTextFragment(paint_fragment->PhysicalFragment());
    if (text_fragment.IsGeneratedText())
      continue;
    // Symbol marker should be appeared in pseudo-element only.
    DCHECK_NE(text_fragment.TextType(), NGPhysicalTextFragment::kSymbolMarker);
    if (last_text_content != text_fragment.TextContent().Impl()) {
      if (!text_boxes.IsEmpty()) {
        ProcessTextBoxes(layout_text, text_boxes);
        text_boxes.clear();
      }
      last_text_content = text_fragment.TextContent().Impl();
    }
    const unsigned start =
        may_have_leading_space && IsAfterWhiteSpace(text_fragment)
            ? text_fragment.StartOffset() - 1
            : text_fragment.StartOffset();
    const unsigned end = text_fragment.EndOffset();
    may_have_leading_space =
        collapse_white_space &&
        !IsCollapsibleSpace(text_fragment.TextContent()[end - 1]);
    text_boxes.emplace_back(
        StringView(text_fragment.TextContent(), start, end - start), start);
  }
  if (!text_boxes.IsEmpty())
    ProcessTextBoxes(layout_text, text_boxes);
  if (!collapse_white_space || !EndsWithWhiteSpace(layout_text))
    return;
  result_.EmitCollapsibleSpace();
}

// The "inner text collection steps".
void ElementInnerTextCollector::ProcessNode(const Node& node) {
  // 1. Let items be the result of running the inner text collection steps with
  // each child node of node in tree order, and then concatenating the results
  // to a single list.

  // 2. If node's computed value of 'visibility' is not 'visible', then return
  // items.
  const ComputedStyle* style = node.GetComputedStyle();
  if (style && style->Visibility() != EVisibility::kVisible)
    return ProcessChildren(node);

  // 3. If node is not being rendered, then return items. For the purpose of
  // this step, the following elements must act as described if the computed
  // value of the 'display' property is not 'none':
  // Note: items can be non-empty due to 'display:contents'.
  if (!IsBeingRendered(node)) {
    // "display:contents" also reaches here since it doesn't have a CSS box.
    return ProcessChildren(node);
  }
  // * select elements have an associated non-replaced inline CSS box whose
  //   child boxes include only those of optgroup and option element child
  //   nodes;
  // * optgroup elements have an associated non-replaced block-level CSS box
  //   whose child boxes include only those of option element child nodes; and
  // * option element have an associated non-replaced block-level CSS box whose
  //   child boxes are as normal for non-replaced block-level CSS boxes.
  if (IsHTMLSelectElement(node))
    return ProcessSelectElement(ToHTMLSelectElement(node));
  if (IsHTMLOptionElement(node)) {
    // Since child nodes of OPTION are not rendered, we use dedicated function.
    // e.g. <div>ab<option>12</div>cd</div>innerText == "ab\n12\ncd"
    // Note: "label" attribute doesn't affect value of innerText.
    return ProcessOptionElement(ToHTMLOptionElement(node));
  }

  // 4. If node is a Text node, then for each CSS text box produced by node.
  if (node.IsTextNode())
    return ProcessTextNode(ToText(node));

  // 5. If node is a br element, then append a string containing a single U+000A
  // LINE FEED (LF) character to items.
  if (IsHTMLBRElement(node)) {
    ProcessChildren(node);
    result_.EmitNewline();
    result_.SetShouldCollapseWhitespace(true);
    return;
  }

  // 6. If node's computed value of 'display' is 'table-cell', and node's CSS
  // box is not the last 'table-cell' box of its enclosing 'table-row' box, then
  // append a string containing a single U+0009 CHARACTER TABULATION (tab)
  // character to items.
  const LayoutObject& layout_object = *node.GetLayoutObject();
  if (style->Display() == EDisplay::kTableCell) {
    ProcessChildrenWithRequiredLineBreaks(node, 0);
    if (layout_object.IsTableCell() &&
        ToLayoutTableCell(layout_object).NextCell())
      result_.EmitTab();
    return;
  }

  // 7. If node's computed value of 'display' is 'table-row', and node's CSS box
  // is not the last 'table-row' box of the nearest ancestor 'table' box, then
  // append a string containing a single U+000A LINE FEED (LF) character to
  // items.
  if (style->Display() == EDisplay::kTableRow) {
    ProcessChildrenWithRequiredLineBreaks(node, 0);
    if (layout_object.IsTableRow() &&
        ShouldEmitNewlineForTableRow(ToLayoutTableRow(layout_object)))
      result_.EmitNewline();
    return;
  }

  // 8. If node is a p element, then append 2 (a required line break count) at
  // the beginning and end of items.
  if (IsHTMLParagraphElement(node)) {
    // Note: <p style="display:contents>foo</p> doesn't generate layout object
    // for P.
    ProcessChildrenWithRequiredLineBreaks(node, 2);
    return;
  }

  // 9. If node's used value of 'display' is block-level or 'table-caption',
  // then append 1 (a required line break count) at the beginning and end of
  // items.
  if (IsDisplayBlockLevel(node))
    return ProcessChildrenWithRequiredLineBreaks(node, 1);

  if (!layout_object.IsAtomicInlineLevel())
    return ProcessChildren(node);

  // We should emit a space before atomic inline item:
  // abc <img> def => "abc  def" See http://crbug.com/894701
  result_.FlushCollapsibleSpace();
  ProcessChildrenWithRequiredLineBreaks(node, 0);
  // We should not collapse white space after inline-block:
  // abc <span style="display:inline-block"></span> def => "abc  def".
  // See http://crbug.com/890020
  result_.SetShouldCollapseWhitespace(false);
}

void ElementInnerTextCollector::ProcessOptionElement(
    const HTMLOptionElement& option_element) {
  result_.EmitRequiredLineBreak(1);
  result_.EmitText(option_element.text());
  result_.EmitRequiredLineBreak(1);
}

void ElementInnerTextCollector::ProcessSelectElement(
    const HTMLSelectElement& select_element) {
  for (const Node& child : NodeTraversal::ChildrenOf(select_element)) {
    if (IsHTMLOptionElement(child)) {
      ProcessOptionElement(ToHTMLOptionElement(child));
      continue;
    }
    if (!IsHTMLOptGroupElement(child))
      continue;
    // Note: We should emit newline for OPTGROUP even if it has no OPTION.
    // e.g. <div>a<select><optgroup></select>b</div>.innerText == "a\nb"
    result_.EmitRequiredLineBreak(1);
    for (const Node& maybe_option : NodeTraversal::ChildrenOf(child)) {
      if (IsHTMLOptionElement(maybe_option))
        ProcessOptionElement(ToHTMLOptionElement(maybe_option));
    }
    result_.EmitRequiredLineBreak(1);
  }
}

void ElementInnerTextCollector::ProcessTextBoxes(
    const LayoutText& layout_text,
    const Vector<TextBox>& passed_text_boxes) {
  DCHECK(!passed_text_boxes.IsEmpty());
  Vector<TextBox> text_boxes = passed_text_boxes;
  // TODO(editing-dev): We may want to check |ContainsReversedText()| in
  // |LayoutText|. See http://crbug.com/873949
  std::sort(text_boxes.begin(), text_boxes.end(),
            [](const TextBox& text_box1, const TextBox& text_box2) {
              return text_box1.start < text_box2.start;
            });
  const EWhiteSpace white_space = layout_text.Style()->WhiteSpace();
  for (const TextBox& text_box : text_boxes)
    ProcessText(text_box.text, white_space);
}

void ElementInnerTextCollector::ProcessText(StringView text,
                                            EWhiteSpace white_space) {
  if (!ComputedStyle::CollapseWhiteSpace(white_space))
    return result_.EmitText(text);
  for (unsigned index = 0; index < text.length(); ++index) {
    if (white_space == EWhiteSpace::kPreLine &&
        text[index] == kNewlineCharacter) {
      result_.EmitNewline();
      continue;
    }
    if (IsCollapsibleSpace(text[index])) {
      result_.EmitCollapsibleSpace();
      continue;
    }
    result_.EmitChar16(text[index]);
  }
}

void ElementInnerTextCollector::ProcessTextNode(const Text& node) {
  if (!node.GetLayoutObject())
    return;
  const LayoutText& layout_text = *node.GetLayoutObject();
  if (LayoutText* first_letter_part = layout_text.GetFirstLetterPart())
    ProcessLayoutText(*first_letter_part);
  ProcessLayoutText(layout_text);
}

// ----

void ElementInnerTextCollector::Result::EmitBeginBlock() {
  should_collapse_white_space_ = true;
}

void ElementInnerTextCollector::Result::EmitChar16(UChar code_point) {
  if (required_line_break_count_ > 0)
    FlushRequiredLineBreak();
  else
    FlushCollapsibleSpace();
  DCHECK_EQ(required_line_break_count_, 0);
  DCHECK(!has_collapsible_space_);
  builder_.Append(code_point);
  should_collapse_white_space_ = false;
}

void ElementInnerTextCollector::Result::EmitCollapsibleSpace() {
  if (should_collapse_white_space_)
    return;
  FlushRequiredLineBreak();
  has_collapsible_space_ = true;
}

void ElementInnerTextCollector::Result::EmitEndBlock() {
  // Discard tailing collapsible spaces from last child of the block.
  has_collapsible_space_ = false;
}

void ElementInnerTextCollector::Result::EmitNewline() {
  FlushRequiredLineBreak();
  has_collapsible_space_ = false;
  builder_.Append(kNewlineCharacter);
}

void ElementInnerTextCollector::Result::EmitRequiredLineBreak(int count) {
  DCHECK_GE(count, 0);
  DCHECK_LE(count, 2);
  if (count == 0)
    return;
  // 4. Remove any runs of consecutive required line break count items at the
  // start or end of results.
  should_collapse_white_space_ = true;
  has_collapsible_space_ = false;
  if (builder_.IsEmpty()) {
    DCHECK_EQ(required_line_break_count_, 0);
    return;
  }
  // 5. Replace each remaining run of consecutive required line break count
  // items with a string consisting of as many U+000A LINE FEED (LF) characters
  // as the maximum of the values in the required line break count items.
  required_line_break_count_ = std::max(required_line_break_count_, count);
}

void ElementInnerTextCollector::Result::EmitTab() {
  if (required_line_break_count_ > 0)
    FlushRequiredLineBreak();
  has_collapsible_space_ = false;
  should_collapse_white_space_ = false;
  builder_.Append(kTabulationCharacter);
}

void ElementInnerTextCollector::Result::EmitText(const StringView& text) {
  if (text.IsEmpty())
    return;
  should_collapse_white_space_ = false;
  if (required_line_break_count_ > 0)
    FlushRequiredLineBreak();
  else
    FlushCollapsibleSpace();
  DCHECK_EQ(required_line_break_count_, 0);
  DCHECK(!has_collapsible_space_);
  builder_.Append(text);
}

String ElementInnerTextCollector::Result::Finish() {
  if (required_line_break_count_ == 0)
    FlushCollapsibleSpace();
  return builder_.ToString();
}

void ElementInnerTextCollector::Result::FlushCollapsibleSpace() {
  if (!has_collapsible_space_)
    return;
  has_collapsible_space_ = false;
  builder_.Append(kSpaceCharacter);
}

void ElementInnerTextCollector::Result::FlushRequiredLineBreak() {
  DCHECK_GE(required_line_break_count_, 0);
  DCHECK_LE(required_line_break_count_, 2);
  builder_.Append("\n\n", required_line_break_count_);
  required_line_break_count_ = 0;
  has_collapsible_space_ = false;
}

}  // anonymous namespace

String Element::innerText() {
  // We need to update layout, since |ElementInnerTextCollector()| uses line
  // boxes in the layout tree.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheetsForNode(this);
  return ElementInnerTextCollector().RunOn(*this);
}

}  // namespace blink
