// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include <algorithm>

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/text_visitor.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_paragraph_element.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node_data.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// The implementation of Element#innerText algorithm[1].
// [1]
// https://html.spec.whatwg.org/C/#the-innertext-idl-attribute
class ElementInnerTextCollector final {
  STACK_ALLOCATED();

 public:
  explicit ElementInnerTextCollector(TextVisitor* visitor)
      : visitor_(visitor) {}
  ElementInnerTextCollector(const ElementInnerTextCollector&) = delete;
  ElementInnerTextCollector& operator=(const ElementInnerTextCollector&) =
      delete;

  String RunOn(const Element& element);

 private:
  // Result characters of innerText collection steps.
  class Result final {
   public:
    Result() = default;
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    void EmitNewline();
    void EmitRequiredLineBreak(wtf_size_t count);
    void EmitTab();
    void EmitText(const StringView& text);
    String Finish();

    unsigned length() const { return builder_.length(); }

   private:
    void FlushRequiredLineBreak();

    StringBuilder builder_;
    wtf_size_t required_line_break_count_ = 0;
  };

  static bool HasDisplayContentsStyle(const Node& node);
  static bool IsBeingRendered(const Node& node);
  // Returns true if used value of "display" is block-level.
  static bool IsDisplayBlockLevel(const Node&);
  static bool ShouldEmitNewlineForTableRow(const LayoutTableRow& table_row);

  const OffsetMapping* GetOffsetMapping(const LayoutText& layout_text);
  void ProcessChildren(const Node& node);
  void ProcessChildrenWithRequiredLineBreaks(
      const Node& node,
      wtf_size_t required_line_break_count);
  void ProcessLayoutText(const LayoutText& layout_text,
                         const Text& text_node,
                         const unsigned start_offset);
  void ProcessTextFromOffsetMapping(const LayoutText& layout_text,
                                    const Text& text_node);
  unsigned ProcessFirstLineAndGetOffset(const LayoutText& layout_text);
  void ProcessNode(const Node& node);
  void ProcessOptionElement(const HTMLOptionElement& element);
  void ProcessOptGroupElement(const HTMLOptGroupElement& element);
  void ProcessSelectElement(const HTMLSelectElement& element);
  void ProcessTextNode(const Text& node);

  // Result character buffer.
  Result result_;
  TextVisitor* visitor_;
};

String ElementInnerTextCollector::RunOn(const Element& element) {
  DCHECK(!element.InActiveDocument() || !NeedsLayoutTreeUpdate(element));

  if (visitor_) {
    visitor_->WillVisit(element, result_.length());
  }

  // 1. If this element is locked or a part of a locked subtree, then it is
  // hidden from view (and also possibly not laid out) and innerText should be
  // empty.
  if (DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(element))
    return {};

  // 2. If this element is not being rendered, or if the user agent is a non-CSS
  // user agent, then return the same value as the textContent IDL attribute on
  // this element.
  // Note: To pass WPT test, case we don't use |textContent| for
  // "display:content". See [1] for discussion about "display:contents" and
  // "being rendered".
  // [1] https://github.com/whatwg/html/issues/1837
  if (!IsBeingRendered(element) && !HasDisplayContentsStyle(element)) {
    const bool convert_brs_to_newlines = false;
    return element.textContent(convert_brs_to_newlines, visitor_);
  }

  // 3. Let results be a new empty list.
  // 4. For each child node node of this element:
  //   1. Let current be the list resulting in running the inner text collection
  //      steps with node. Each item in results will either be a JavaScript
  //      string or a positive integer (a required line break count).
  //   2. For each item item in current, append item to results.
  // Note: Handles <select> and <option> here since they are implemented as
  // UA shadow DOM, e.g. Text nodes in <option> don't have layout object.
  // See also: https://github.com/whatwg/html/issues/3797
  if (auto* html_select_element = DynamicTo<HTMLSelectElement>(element))
    ProcessSelectElement(*html_select_element);
  else if (auto* option_element = DynamicTo<HTMLOptionElement>(element))
    ProcessOptionElement(*option_element);
  else
    ProcessChildren(element);
  return result_.Finish();
}

// static
bool ElementInnerTextCollector::HasDisplayContentsStyle(const Node& node) {
  auto* element = DynamicTo<Element>(node);
  return element && element->HasDisplayContentsStyle();
}

// An element is *being rendered* if it has any associated CSS layout boxes,
// SVG layout boxes, or some equivalent in other styling languages.
// Note: Just being off-screen does not mean the element is not being rendered.
// The presence of the "hidden" attribute normally means the element is not
// being rendered, though this might be overridden by the style sheets.
// From https://html.spec.whatwg.org/C/#being-rendered
// static
bool ElementInnerTextCollector::IsBeingRendered(const Node& node) {
  return node.GetLayoutObject();
}

// static
bool ElementInnerTextCollector::IsDisplayBlockLevel(const Node& node) {
  const LayoutObject* const layout_object = node.GetLayoutObject();
  if (!layout_object)
    return false;
  if (layout_object->IsTableSection()) {
    // Note: |LayoutTableSection::IsInline()| returns false, but it is not
    // block-level.
    return false;
  }
  if (!layout_object->IsLayoutBlock()) {
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
  // Note: CAPTION is associated to |LayoutTableCaption| in LayoutNG or
  // |LayoutBlockFlow| in legacy layout.
  return true;
}

// static
bool ElementInnerTextCollector::ShouldEmitNewlineForTableRow(
    const LayoutTableRow& table_row) {
  const LayoutTable* const table = table_row.Table();
  if (!table)
    return false;
  if (table_row.NextRow()) {
    return true;
  }
  // For TABLE contains TBODY, TFOOTER, THEAD.
  const LayoutTableSection* table_section = table_row.Section();
  if (!table_section)
    return false;
  // See |LayoutTable::NextSection()| and
  // |PreviousSection()| for traversing |LayoutTableSection|.
  for (const LayoutObject* runner = table_section->NextSibling(); runner;
       runner = runner->NextSibling()) {
    const auto* section = DynamicTo<LayoutTableSection>(runner);
    if (section && section->NumRows() > 0) {
      return true;
    }
  }
  // No table row after |node|.
  return false;
}

const OffsetMapping* ElementInnerTextCollector::GetOffsetMapping(
    const LayoutText& layout_text) {
  // TODO(editing-dev): We should handle "text-transform" in "::first-line".
  // In legacy layout, |InlineTextBox| holds original text and text box
  // paint does text transform.
  LayoutBlockFlow* const block_flow =
      OffsetMapping::GetInlineFormattingContextOf(layout_text);
  DCHECK(block_flow) << layout_text;
  return InlineNode::GetOffsetMapping(block_flow);
}

void ElementInnerTextCollector::ProcessChildren(const Node& container) {
  for (const Node& node : NodeTraversal::ChildrenOf(container)) {
    if (visitor_) {
      visitor_->WillVisit(node, result_.length());
    }
    ProcessNode(node);
  }
}

void ElementInnerTextCollector::ProcessChildrenWithRequiredLineBreaks(
    const Node& node,
    wtf_size_t required_line_break_count) {
  DCHECK_GE(required_line_break_count, 1u);
  DCHECK_LE(required_line_break_count, 2u);
  result_.EmitRequiredLineBreak(required_line_break_count);
  ProcessChildren(node);
  result_.EmitRequiredLineBreak(required_line_break_count);
}

void ElementInnerTextCollector::ProcessLayoutText(const LayoutText& layout_text,
                                                  const Text& text_node,
                                                  const unsigned start_offset) {
  if (layout_text.HasEmptyText()) {
    return;
  }
  if (layout_text.Style()->Visibility() != EVisibility::kVisible) {
    // TODO(editing-dev): Once we make ::first-letter don't apply "visibility",
    // we should get rid of this if-statement. http://crbug.com/866744
    return;
  }

  // LayoutText::PlainText() gives the rendered text after the application
  // of white-space processing and text-transform rules
  if (RuntimeEnabledFeatures::ElementInnerTextHandleFirstLineStyleEnabled()) {
    const ComputedStyle* block_style = layout_text.Style();
    const ComputedStyle* first_line_style = layout_text.FirstLineStyle();

    // first_line_offset is the first character of the text that is not part of
    // ::first_line
    unsigned first_line_offset = 0;
    if (block_style->TextTransform() != first_line_style->TextTransform()) {
      first_line_offset = ProcessFirstLineAndGetOffset(layout_text);
    }
    const unsigned adjusted_offset =
        first_line_offset ? first_line_offset : start_offset;
    const String plain_text = layout_text.PlainText();
    const unsigned text_length = plain_text.length();
    if (adjusted_offset < text_length) {
      result_.EmitText(StringView(plain_text, adjusted_offset,
                                  text_length - adjusted_offset));
    }
  } else {
    ProcessTextFromOffsetMapping(layout_text, text_node);
  }
}

void ElementInnerTextCollector::ProcessTextFromOffsetMapping(
    const LayoutText& layout_text,
    const Text& text_node) {
  const OffsetMapping* const mapping = GetOffsetMapping(layout_text);
  if (!mapping) {
    // TODO(crbug.com/967995): There are certain cases where we fail to
    // compute |OffsetMapping| due to failures in layout. As the root cause is
    // hard to fix at the moment, we work around it here so that the
    // production build doesn't crash.
    DUMP_WILL_BE_NOTREACHED() << layout_text;
    return;
  }

  for (const OffsetMappingUnit& unit :
       mapping->GetMappingUnitsForNode(text_node)) {
    result_.EmitText(
        StringView(mapping->GetText(), unit.TextContentStart(),
                   unit.TextContentEnd() - unit.TextContentStart()));
  }
}

// Offset mappings don't have text offsets for ::first-line. Get the rendered
// text for ::first-line from FragmentItems and return the length of
// the ::first-line part as offset
unsigned ElementInnerTextCollector::ProcessFirstLineAndGetOffset(
    const LayoutText& layout_text) {
  LayoutBlockFlow* const block_flow = layout_text.FragmentItemsContainer();
  DCHECK(block_flow) << layout_text;
  unsigned first_line_length = 0;
  for (InlineCursor cursor(*block_flow);
       cursor && cursor.Current().UsesFirstLineStyle(); cursor.MoveToNext()) {
    if (!cursor.CurrentItem()->IsText()) {
      continue;
    }
    if (To<LayoutText>(cursor.Current().GetLayoutObject()) == &layout_text) {
      result_.EmitText(cursor.Current().Text(cursor));
      first_line_length = cursor.Current().TextEndOffset();
    }
  }
  return first_line_length;
}

// The "inner text collection steps".
void ElementInnerTextCollector::ProcessNode(const Node& node) {
  // 1. Let items be the result of running the inner text collection steps with
  // each child node of node in tree order, and then concatenating the results
  // to a single list.

  // 2. If the node is display locked, then we should not process it or its
  // children, since they are not visible or accessible via innerText.
  if (DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(node))
    return;

  // 3. If node's computed value of 'visibility' is not 'visible', then return
  // items.
  const ComputedStyle* style = GetComputedStyleForElementOrLayoutObject(node);
  if (style && style->Visibility() != EVisibility::kVisible) {
    return ProcessChildren(node);
  }

  // 4. If node is not being rendered, then return items. For the purpose of
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
  if (auto* html_select_element = DynamicTo<HTMLSelectElement>(node))
    return ProcessSelectElement(*html_select_element);
  if (auto* option_element = DynamicTo<HTMLOptionElement>(node)) {
    // Since child nodes of OPTION are not rendered, we use dedicated function.
    // e.g. <div>ab<option>12</div>cd</div>innerText == "ab\n12\ncd"
    // Note: "label" attribute doesn't affect value of innerText.
    return ProcessOptionElement(*option_element);
  }

  // 5. If node is a Text node, then for each CSS text box produced by node.
  auto* text_node = DynamicTo<Text>(node);
  if (text_node)
    return ProcessTextNode(*text_node);

  // 6. If node is a br element, then append a string containing a single U+000A
  // LINE FEED (LF) character to items.
  if (IsA<HTMLBRElement>(node)) {
    ProcessChildren(node);
    result_.EmitNewline();
    return;
  }

  // 7. If node's computed value of 'display' is 'table-cell', and node's CSS
  // box is not the last 'table-cell' box of its enclosing 'table-row' box, then
  // append a string containing a single U+0009 CHARACTER TABULATION (tab)
  // character to items.
  const LayoutObject& layout_object = *node.GetLayoutObject();
  if (style->Display() == EDisplay::kTableCell) {
    ProcessChildren(node);
    if (layout_object.IsTableCell() && layout_object.NextSibling()) {
      result_.EmitTab();
    }
    return;
  }

  // 8. If node's computed value of 'display' is 'table-row', and node's CSS box
  // is not the last 'table-row' box of the nearest ancestor 'table' box, then
  // append a string containing a single U+000A LINE FEED (LF) character to
  // items.
  if (style->Display() == EDisplay::kTableRow) {
    ProcessChildren(node);
    if (layout_object.IsTableRow() &&
        ShouldEmitNewlineForTableRow(To<LayoutTableRow>(layout_object))) {
      result_.EmitNewline();
    }
    return;
  }

  // 9. If node is a p element, then append 2 (a required line break count) at
  // the beginning and end of items.
  if (IsA<HTMLParagraphElement>(node)) {
    // Note: <p style="display:contents>foo</p> doesn't generate layout object
    // for P.
    ProcessChildrenWithRequiredLineBreaks(node, 2u);
    return;
  }

  // 10. If node's used value of 'display' is block-level or 'table-caption',
  // then append 1 (a required line break count) at the beginning and end of
  // items.
  if (IsDisplayBlockLevel(node))
    return ProcessChildrenWithRequiredLineBreaks(node, 1u);

  ProcessChildren(node);
}

void ElementInnerTextCollector::ProcessOptionElement(
    const HTMLOptionElement& option_element) {
  result_.EmitRequiredLineBreak(1);
  result_.EmitText(option_element.text());
  result_.EmitRequiredLineBreak(1);
}

void ElementInnerTextCollector::ProcessOptGroupElement(
    const HTMLOptGroupElement& optgroup) {
  // Note: We should emit newline for OPTGROUP even if it has no OPTION.
  // e.g. <div>a<select><optgroup></select>b</div>.innerText == "a\nb"
  result_.EmitRequiredLineBreak(1);
  Element* descendant = ElementTraversal::FirstChild(optgroup);
  while (descendant) {
    if (visitor_) {
      visitor_->WillVisit(*descendant, result_.length());
    }
    // TODO(crbug.com/389573453): Consider handling <hr> elements here.
    if (auto* option = DynamicTo<HTMLOptionElement>(descendant)) {
      ProcessOptionElement(*option);
      descendant =
          ElementTraversal::NextSkippingChildren(*descendant, &optgroup);
    } else if (IsA<HTMLOptGroupElement>(descendant)) {
      // TODO(crbug.com/389573453): Consider adding nested <optgroup>s here. For
      // now we will skip them.
      descendant =
          ElementTraversal::NextSkippingChildren(*descendant, &optgroup);
    } else {
      descendant = ElementTraversal::Next(*descendant, &optgroup);
    }
  }
  result_.EmitRequiredLineBreak(1);
}

void ElementInnerTextCollector::ProcessSelectElement(
    const HTMLSelectElement& select_element) {
  // TODO(crbug.com/40271842): Consider Handling display:none on various
  // elements here, especially options.
  Element* descendant = ElementTraversal::FirstChild(select_element);
  while (descendant) {
    if (visitor_) {
      visitor_->WillVisit(*descendant, result_.length());
    }
    // TODO(crbug.com/389573453): Consider handling <hr> elements here.
    if (auto* option = DynamicTo<HTMLOptionElement>(descendant)) {
      ProcessOptionElement(*option);
      descendant =
          ElementTraversal::NextSkippingChildren(*descendant, &select_element);
    } else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(descendant)) {
      ProcessOptGroupElement(*optgroup);
      descendant =
          ElementTraversal::NextSkippingChildren(*descendant, &select_element);
    } else {
      descendant = ElementTraversal::Next(*descendant, &select_element);
    }
  }
}

void ElementInnerTextCollector::ProcessTextNode(const Text& node) {
  if (!node.GetLayoutObject())
    return;
  const LayoutText& layout_text = *node.GetLayoutObject();
  if (LayoutText* first_letter_part = layout_text.GetFirstLetterPart()) {
    if (layout_text.HasEmptyText() ||
        OffsetMapping::GetInlineFormattingContextOf(layout_text) !=
            OffsetMapping::GetInlineFormattingContextOf(*first_letter_part)) {
      // "::first-letter" with "float" reach here.
      ProcessLayoutText(*first_letter_part, node, 0);
      unsigned first_letter_length = first_letter_part->PlainText().length();
      ProcessLayoutText(layout_text, node, first_letter_length);
      return;
    }
  }
  ProcessLayoutText(layout_text, node, 0);
}

// ----

void ElementInnerTextCollector::Result::EmitNewline() {
  FlushRequiredLineBreak();
  builder_.Append(uchar::kLineFeed);
}

void ElementInnerTextCollector::Result::EmitRequiredLineBreak(
    wtf_size_t count) {
  DCHECK_LE(count, 2u);
  if (count == 0)
    return;
  // 4. Remove any runs of consecutive required line break count items at the
  // start or end of results.
  if (builder_.empty()) {
    DCHECK_EQ(required_line_break_count_, 0u);
    return;
  }
  // 5. Replace each remaining run of consecutive required line break count
  // items with a string consisting of as many U+000A LINE FEED (LF) characters
  // as the maximum of the values in the required line break count items.
  required_line_break_count_ = std::max(required_line_break_count_, count);
}

void ElementInnerTextCollector::Result::EmitTab() {
  FlushRequiredLineBreak();
  builder_.Append(uchar::kTab);
}

void ElementInnerTextCollector::Result::EmitText(const StringView& text) {
  if (text.empty())
    return;
  FlushRequiredLineBreak();
  DCHECK_EQ(required_line_break_count_, 0u);
  builder_.Append(text);
}

String ElementInnerTextCollector::Result::Finish() {
  return builder_.ToString();
}

void ElementInnerTextCollector::Result::FlushRequiredLineBreak() {
  DCHECK_LE(required_line_break_count_, 2u);
  builder_.Append(
      base::byte_span_from_cstring("\n\n").first(required_line_break_count_));
  required_line_break_count_ = 0;
}

}  // anonymous namespace

String Element::innerText(TextVisitor* visitor) {
  // We need to update layout, since |ElementInnerTextCollector()| uses line
  // boxes in the layout tree.
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  return GetInnerTextWithoutUpdate(visitor);
}

// Used for callers that must ensure no document lifecycle rewind.
String Element::GetInnerTextWithoutUpdate(TextVisitor* visitor) {
  // TODO(https:://crbug.com/1165850 https:://crbug.com/1166296) Layout should
  // always be clean here, but the lifecycle does not report the correctly
  // updated value unless servicing animations. Fix the UpdateStyleAndLayout()
  // to correctly advance the lifecycle, and then update the following DCHECK to
  // always require clean layout in active documents.
  // DCHECK(!GetDocument().IsActive() || !GetDocument().GetPage() ||
  //        GetDocument().Lifecycle().GetState() >=
  //            DocumentLifecycle::kLayoutClean)
  //     << "Layout must be clean when GetInnerTextWithoutUpdate() is called.";
  return ElementInnerTextCollector(visitor).RunOn(*this);
}

}  // namespace blink
