/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008, 2009, 2010, 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/serializers/styled_markup_accumulator.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

wtf_size_t TotalLength(const Vector<String>& strings) {
  wtf_size_t length = 0;
  for (const auto& string : strings)
    length += string.length();
  return length;
}

}  // namespace

StyledMarkupAccumulator::StyledMarkupAccumulator(
    const TextOffset& start,
    const TextOffset& end,
    Document* document,
    const CreateMarkupOptions& options)
    : formatter_(options.ShouldResolveURLs(),
                 IsA<HTMLDocument>(document) ? SerializationType::kHTML
                                             : SerializationType::kXML),
      start_(start),
      end_(end),
      document_(document),
      options_(options) {}

void StyledMarkupAccumulator::AppendEndTag(const Element& element) {
  AppendEndMarkup(result_, element);
}

void StyledMarkupAccumulator::AppendStartMarkup(Node& node) {
  formatter_.AppendStartMarkup(result_, node);
}

void StyledMarkupAccumulator::AppendEndMarkup(StringBuilder& result,
                                              const Element& element) {
  formatter_.AppendEndMarkup(result, element);
}

void StyledMarkupAccumulator::AppendText(Text& text) {
  const String& str = text.data();
  unsigned length = str.length();
  unsigned start = 0;
  if (end_.IsNotNull()) {
    if (text == end_.GetText())
      length = end_.Offset();
  }
  if (start_.IsNotNull()) {
    if (text == start_.GetText()) {
      start = start_.Offset();
      length -= start;
    }
  }
  MarkupFormatter::AppendCharactersReplacingEntities(
      result_, StringView(str, start, length),
      formatter_.EntityMaskForText(text));
}

void StyledMarkupAccumulator::AppendTextWithInlineStyle(
    Text& text,
    EditingStyle* inline_style) {
  if (inline_style) {
    // wrappingStyleForAnnotatedSerialization should have removed
    // -webkit-text-decorations-in-effect.
    DCHECK(!ShouldAnnotate() ||
           PropertyMissingOrEqualToNone(
               inline_style->Style(),
               CSSPropertyID::kWebkitTextDecorationsInEffect));
    DCHECK(document_);

    result_.Append("<span style=\"");
    MarkupFormatter::AppendAttributeValue(
        result_, inline_style->Style()->AsText(), IsA<HTMLDocument>(document_),
        *document_);
    result_.Append("\">");
  }
  if (!ShouldAnnotate()) {
    AppendText(text);
  } else {
    const bool use_rendered_text = !EnclosingElementWithTag(
        Position::FirstPositionInNode(text), html_names::kSelectTag);
    String content =
        use_rendered_text ? RenderedText(text) : StringValueForRange(text);
    StringBuilder buffer;
    MarkupFormatter::AppendCharactersReplacingEntities(buffer, content,
                                                       kEntityMaskInPCDATA);
    // Keep collapsible white spaces as is during markup sanitization.
    const String text_to_append =
        IsForMarkupSanitization()
            ? buffer.ToString()
            : ConvertHTMLTextToInterchangeFormat(buffer.ToString(), text);
    result_.Append(text_to_append);
  }
  if (inline_style)
    result_.Append("</span>");
}

void StyledMarkupAccumulator::AppendElementWithInlineStyle(
    const Element& element,
    EditingStyle* style) {
  AppendElementWithInlineStyle(result_, element, style);
}

void StyledMarkupAccumulator::AppendElementWithInlineStyle(
    StringBuilder& out,
    const Element& element,
    EditingStyle* style) {
  const bool document_is_html = IsA<HTMLDocument>(element.GetDocument());
  formatter_.AppendStartTagOpen(out, element);
  AttributeCollection attributes = element.Attributes();
  for (const auto& attribute : attributes) {
    // We'll handle the style attribute separately, below.
    if (attribute.GetName() == html_names::kStyleAttr)
      continue;
    AppendAttribute(out, element, attribute);
  }
  if (style && !style->IsEmpty()) {
    out.Append(" style=\"");
    MarkupFormatter::AppendAttributeValue(
        out, style->Style()->AsText(), document_is_html, element.GetDocument());
    out.Append('\"');
  }
  formatter_.AppendStartTagClose(out, element);
}

void StyledMarkupAccumulator::AppendElement(const Element& element) {
  AppendElement(result_, element);
}

void StyledMarkupAccumulator::AppendElement(StringBuilder& out,
                                            const Element& element) {
  formatter_.AppendStartTagOpen(out, element);
  AttributeCollection attributes = element.Attributes();
  for (const auto& attribute : attributes)
    AppendAttribute(out, element, attribute);
  formatter_.AppendStartTagClose(out, element);
}

void StyledMarkupAccumulator::AppendAttribute(StringBuilder& result,
                                              const Element& element,
                                              const Attribute& attribute) {
  String value = formatter_.ResolveURLIfNeeded(element, attribute);
  if (formatter_.SerializeAsHTML()) {
    MarkupFormatter::AppendAttributeAsHTML(result, attribute, value,
                                           element.GetDocument());
  } else {
    MarkupFormatter::AppendAttributeAsXMLWithoutNamespace(
        result, attribute, value, element.GetDocument());
  }
}

void StyledMarkupAccumulator::WrapWithStyleNode(CSSPropertyValueSet* style) {
  // wrappingStyleForSerialization should have removed
  // -webkit-text-decorations-in-effect.
  DCHECK(PropertyMissingOrEqualToNone(
      style, CSSPropertyID::kWebkitTextDecorationsInEffect));
  DCHECK(document_);

  StringBuilder open_tag;
  open_tag.Append("<div style=\"");
  MarkupFormatter::AppendAttributeValue(
      open_tag, style->AsText(), IsA<HTMLDocument>(document_), *document_);
  open_tag.Append("\">");
  reversed_preceding_markup_.push_back(open_tag.ToString());

  result_.Append("</div>");
}

String StyledMarkupAccumulator::TakeResults() {
  StringBuilder result;
  result.ReserveCapacity(TotalLength(reversed_preceding_markup_) +
                         result_.length());

  for (wtf_size_t i = reversed_preceding_markup_.size(); i > 0; --i)
    result.Append(reversed_preceding_markup_[i - 1]);

  result.Append(result_);

  // We remove '\0' characters because they are not visibly rendered to the
  // user.
  return result.ToString().Replace(0, "");
}

String StyledMarkupAccumulator::RenderedText(Text& text_node) {
  int start_offset = 0;
  int end_offset = text_node.length();
  if (start_.GetText() == text_node)
    start_offset = start_.Offset();
  if (end_.GetText() == text_node)
    end_offset = end_.Offset();

  return PlainText(EphemeralRange(Position(&text_node, start_offset),
                                  Position(&text_node, end_offset)),
                   TextIteratorBehavior::Builder()
                       .SetIgnoresCSSTextTransforms(
                           options_.IgnoresCSSTextTransformsForRenderedText())
                       .Build());
}

String StyledMarkupAccumulator::StringValueForRange(const Text& node) {
  if (start_.IsNull())
    return node.data();

  String str = node.data();
  if (start_.GetText() == node)
    str.Truncate(end_.Offset());
  if (end_.GetText() == node)
    str.Remove(0, start_.Offset());
  return str;
}

bool StyledMarkupAccumulator::ShouldAnnotate() const {
  return options_.ShouldAnnotateForInterchange();
}

void StyledMarkupAccumulator::PushMarkup(const String& str) {
  reversed_preceding_markup_.push_back(str);
}

void StyledMarkupAccumulator::AppendInterchangeNewline() {
  DEFINE_STATIC_LOCAL(const String, interchange_newline_string,
                      ("<br class=\"" AppleInterchangeNewline "\">"));
  result_.Append(interchange_newline_string);
}

}  // namespace blink
