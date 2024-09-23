/*
 * Copyright (C) 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/html_view_source_document.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_base_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/parser/html_view_source_parser.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

class ViewSourceEventListener : public NativeEventListener {
 public:
  ViewSourceEventListener(HTMLTableElement* table, HTMLInputElement* checkbox)
      : table_(table), checkbox_(checkbox) {}

  void Invoke(ExecutionContext*, Event* event) override {
    DCHECK_EQ(event->type(), event_type_names::kChange);
    table_->setAttribute(html_names::kClassAttr, checkbox_->Checked()
                                                     ? AtomicString("line-wrap")
                                                     : g_empty_atom);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(table_);
    visitor->Trace(checkbox_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<HTMLTableElement> table_;
  Member<HTMLInputElement> checkbox_;
};

HTMLViewSourceDocument::HTMLViewSourceDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer), type_(initializer.GetMimeType()) {
  SetIsViewSource(true);
  SetCompatibilityMode(kNoQuirksMode);
  LockCompatibilityMode();
}

DocumentParser* HTMLViewSourceDocument::CreateParser() {
  return MakeGarbageCollected<HTMLViewSourceParser>(*this, type_);
}

void HTMLViewSourceDocument::CreateContainingTable() {
  auto* html = MakeGarbageCollected<HTMLHtmlElement>(*this);
  ParserAppendChild(html);
  auto* head = MakeGarbageCollected<HTMLHeadElement>(*this);
  auto* meta =
      MakeGarbageCollected<HTMLMetaElement>(*this, CreateElementFlags());
  meta->setAttribute(html_names::kNameAttr, keywords::kColorScheme);
  meta->setAttribute(html_names::kContentAttr, AtomicString("light dark"));
  head->ParserAppendChild(meta);
  html->ParserAppendChild(head);
  auto* body = MakeGarbageCollected<HTMLBodyElement>(*this);
  html->ParserAppendChild(body);

  // Create a line gutter div that can be used to make sure the gutter extends
  // down the height of the whole document.
  auto* div = MakeGarbageCollected<HTMLDivElement>(*this);
  div->setAttribute(html_names::kClassAttr,
                    AtomicString("line-gutter-backdrop"));
  body->ParserAppendChild(div);

  auto* table = MakeGarbageCollected<HTMLTableElement>(*this);
  tbody_ = MakeGarbageCollected<HTMLTableSectionElement>(html_names::kTbodyTag,
                                                         *this);
  table->ParserAppendChild(tbody_);
  current_ = tbody_;
  line_number_ = 0;

  // Create a checkbox to control line wrapping.
  auto* checkbox = MakeGarbageCollected<HTMLInputElement>(*this);
  checkbox->setAttribute(html_names::kTypeAttr, input_type_names::kCheckbox);
  checkbox->addEventListener(
      event_type_names::kChange,
      MakeGarbageCollected<ViewSourceEventListener>(table, checkbox),
      /*use_capture=*/false);
  checkbox->setAttribute(html_names::kAriaLabelAttr, WTF::AtomicString(Locale::DefaultLocale().QueryString(
                              IDS_VIEW_SOURCE_LINE_WRAP)));
  auto* label = MakeGarbageCollected<HTMLLabelElement>(*this);
  label->ParserAppendChild(
      Text::Create(*this, WTF::AtomicString(Locale::DefaultLocale().QueryString(
                              IDS_VIEW_SOURCE_LINE_WRAP))));
  label->setAttribute(html_names::kClassAttr,
                      AtomicString("line-wrap-control"));
  label->ParserAppendChild(checkbox);
  // Add the checkbox to a form with autocomplete=off, to avoid form
  // restoration from changing the value of the checkbox.
  auto* form = MakeGarbageCollected<HTMLFormElement>(*this);
  form->setAttribute(html_names::kAutocompleteAttr, AtomicString("off"));
  form->ParserAppendChild(label);
  body->ParserAppendChild(form);
  body->ParserAppendChild(table);
}

void HTMLViewSourceDocument::AddSource(
    const String& source,
    HTMLToken& token,
    const HTMLAttributesRanges& attributes_ranges,
    int token_start) {
  if (!current_)
    CreateContainingTable();

  switch (token.GetType()) {
    case HTMLToken::kUninitialized:
      NOTREACHED_IN_MIGRATION();
      break;
    case HTMLToken::DOCTYPE:
      ProcessDoctypeToken(source, token);
      break;
    case HTMLToken::kEndOfFile:
      ProcessEndOfFileToken(source, token);
      break;
    case HTMLToken::kStartTag:
    case HTMLToken::kEndTag:
      ProcessTagToken(source, token, attributes_ranges, token_start);
      break;
    case HTMLToken::kComment:
      ProcessCommentToken(source, token);
      break;
    case HTMLToken::kCharacter:
    case HTMLToken::kDOMPart:
      // Process DOM Parts as character tokens.
      ProcessCharacterToken(source, token);
      break;
  }
}

void HTMLViewSourceDocument::ProcessDoctypeToken(const String& source,
                                                 HTMLToken&) {
  current_ = AddSpanWithClassName(class_doctype_);
  AddText(source, class_doctype_);
  current_ = td_;
}

void HTMLViewSourceDocument::ProcessEndOfFileToken(const String& source,
                                                   HTMLToken&) {
  current_ = AddSpanWithClassName(class_end_of_file_);
  AddText(source, class_end_of_file_);
  current_ = td_;
}

void HTMLViewSourceDocument::ProcessTagToken(
    const String& source,
    const HTMLToken& token,
    const HTMLAttributesRanges& attributes_ranges,
    int token_start) {
  current_ = AddSpanWithClassName(class_tag_);

  AtomicString tag_name = token.GetName().AsAtomicString();

  unsigned index = 0;
  wtf_size_t attribute_index = 0;
  DCHECK_EQ(token.Attributes().size(), attributes_ranges.attributes().size());
  while (index < source.length()) {
    if (attribute_index == attributes_ranges.attributes().size()) {
      // We want to show the remaining characters in the token.
      index = AddRange(source, index, source.length(), g_empty_atom);
      DCHECK_EQ(index, source.length());
      break;
    }

    const HTMLToken::Attribute& attribute = token.Attributes()[attribute_index];
    const AtomicString name(attribute.GetName());
    const AtomicString value(attribute.GetValue());

    const HTMLAttributesRanges::Attribute& attribute_range =
        attributes_ranges.attributes()[attribute_index];

    index =
        AddRange(source, index, attribute_range.name_range.start - token_start,
                 g_empty_atom);
    index =
        AddRange(source, index, attribute_range.name_range.end - token_start,
                 class_attribute_name_);

    if (tag_name == html_names::kBaseTag && name == html_names::kHrefAttr)
      AddBase(value);

    index =
        AddRange(source, index, attribute_range.value_range.start - token_start,
                 g_empty_atom);

    if (name == html_names::kSrcsetAttr) {
      index = AddSrcset(source, index,
                        attribute_range.value_range.end - token_start);
    } else {
      bool is_link =
          name == html_names::kSrcAttr || name == html_names::kHrefAttr;
      index =
          AddRange(source, index, attribute_range.value_range.end - token_start,
                   class_attribute_value_, is_link,
                   tag_name == html_names::kATag, value);
    }

    ++attribute_index;
  }
  current_ = td_;
}

void HTMLViewSourceDocument::ProcessCommentToken(const String& source,
                                                 HTMLToken&) {
  current_ = AddSpanWithClassName(class_comment_);
  AddText(source, class_comment_);
  current_ = td_;
}

void HTMLViewSourceDocument::ProcessCharacterToken(const String& source,
                                                   HTMLToken&) {
  AddText(source, g_empty_atom);
}

Element* HTMLViewSourceDocument::AddSpanWithClassName(
    const AtomicString& class_name) {
  if (current_ == tbody_) {
    AddLine(class_name);
    return current_.Get();
  }

  auto* span = MakeGarbageCollected<HTMLSpanElement>(*this);
  span->setAttribute(html_names::kClassAttr, class_name);
  current_->ParserAppendChild(span);
  return span;
}

void HTMLViewSourceDocument::AddLine(const AtomicString& class_name) {
  // Create a table row.
  auto* trow = MakeGarbageCollected<HTMLTableRowElement>(*this);
  tbody_->ParserAppendChild(trow);

  // Create a cell that will hold the line number (it is generated in the
  // stylesheet using counters).
  auto* td =
      MakeGarbageCollected<HTMLTableCellElement>(html_names::kTdTag, *this);
  td->setAttribute(html_names::kClassAttr, AtomicString("line-number"));
  td->SetIntegralAttribute(html_names::kValueAttr, ++line_number_);
  trow->ParserAppendChild(td);

  // Create a second cell for the line contents
  td = MakeGarbageCollected<HTMLTableCellElement>(html_names::kTdTag, *this);
  td->setAttribute(html_names::kClassAttr, AtomicString("line-content"));
  trow->ParserAppendChild(td);
  current_ = td_ = td;

  // Open up the needed spans.
  if (!class_name.empty()) {
    if (class_name == "html-attribute-name" ||
        class_name == "html-attribute-value")
      current_ = AddSpanWithClassName(class_tag_);
    current_ = AddSpanWithClassName(class_name);
  }
}

void HTMLViewSourceDocument::FinishLine() {
  if (!current_->HasChildren()) {
    auto* br = MakeGarbageCollected<HTMLBRElement>(*this);
    current_->ParserAppendChild(br);
  }
  current_ = tbody_;
}

void HTMLViewSourceDocument::AddText(const String& text,
                                     const AtomicString& class_name) {
  if (text.empty())
    return;

  // Add in the content, splitting on linebreaks.
  // \r and \n both count as linebreaks, but \r\n only counts as one linebreak.
  Vector<String> lines;
  {
    unsigned start_pos = 0;
    unsigned pos = 0;
    while (pos < text.length()) {
      if (text[pos] == '\r') {
        lines.push_back(text.Substring(start_pos, pos - start_pos));
        pos++;
        if (pos < text.length() && text[pos] == '\n') {
          pos++;  // \r\n counts as a single line break.
        }
        start_pos = pos;
      } else if (text[pos] == '\n') {
        lines.push_back(text.Substring(start_pos, pos - start_pos));
        pos++;
        start_pos = pos;
      } else {
        pos++;
      }
    }
    lines.push_back(text.Substring(start_pos, text.length() - start_pos));
  }

  unsigned size = lines.size();
  for (unsigned i = 0; i < size; i++) {
    String substring = lines[i];
    if (current_ == tbody_)
      AddLine(class_name);
    if (substring.empty()) {
      if (i == size - 1)
        break;
      FinishLine();
      continue;
    }
    Element* old_element = current_;
    current_->ParserAppendChild(Text::Create(*this, substring));
    current_ = old_element;
    if (i < size - 1)
      FinishLine();
  }
}

int HTMLViewSourceDocument::AddRange(const String& source,
                                     int start,
                                     int end,
                                     const AtomicString& class_name,
                                     bool is_link,
                                     bool is_anchor,
                                     const AtomicString& link) {
  DCHECK_LE(start, end);
  if (start == end)
    return start;

  String text = source.Substring(start, end - start);
  if (!class_name.empty()) {
    if (is_link)
      current_ = AddLink(link, is_anchor);
    else
      current_ = AddSpanWithClassName(class_name);
  }
  AddText(text, class_name);
  if (!class_name.empty() && current_ != tbody_)
    current_ = To<Element>(current_->parentNode());
  return end;
}

Element* HTMLViewSourceDocument::AddBase(const AtomicString& href) {
  auto* base = MakeGarbageCollected<HTMLBaseElement>(*this);
  base->setAttribute(html_names::kHrefAttr, href);
  current_->ParserAppendChild(base);
  return base;
}

Element* HTMLViewSourceDocument::AddLink(const AtomicString& url,
                                         bool is_anchor) {
  if (current_ == tbody_)
    AddLine(class_tag_);

  // Now create a link for the attribute value instead of a span.
  auto* anchor = MakeGarbageCollected<HTMLAnchorElement>(*this);
  const char* class_value;
  if (is_anchor)
    class_value = "html-attribute-value html-external-link";
  else
    class_value = "html-attribute-value html-resource-link";
  anchor->setAttribute(html_names::kClassAttr, AtomicString(class_value));
  anchor->setAttribute(html_names::kTargetAttr, AtomicString("_blank"));
  anchor->setAttribute(html_names::kHrefAttr, url);
  anchor->setAttribute(html_names::kRelAttr,
                       AtomicString("noreferrer noopener"));
  // Disallow JavaScript hrefs. https://crbug.com/808407
  if (anchor->Url().ProtocolIsJavaScript())
    anchor->setAttribute(html_names::kHrefAttr, AtomicString("about:blank"));
  current_->ParserAppendChild(anchor);
  return anchor;
}

int HTMLViewSourceDocument::AddSrcset(const String& source,
                                      int start,
                                      int end) {
  String srcset = source.Substring(start, end - start);
  Vector<String> srclist;
  srcset.Split(',', true, srclist);
  unsigned size = srclist.size();
  for (unsigned i = 0; i < size; i++) {
    Vector<String> tmp;
    srclist[i].Split(' ', tmp);
    if (tmp.size() > 0) {
      AtomicString link(tmp[0]);
      current_ = AddLink(link, false);
      AddText(srclist[i], class_attribute_value_);
      current_ = To<Element>(current_->parentNode());
    } else {
      AddText(srclist[i], class_attribute_value_);
    }
    if (i + 1 < size)
      AddText(",", class_attribute_value_);
  }
  return end;
}

void HTMLViewSourceDocument::Trace(Visitor* visitor) const {
  visitor->Trace(current_);
  visitor->Trace(tbody_);
  visitor->Trace(td_);
  HTMLDocument::Trace(visitor);
}

}  // namespace blink
