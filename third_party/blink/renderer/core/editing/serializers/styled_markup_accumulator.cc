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
    : formatter_(options.ShouldResolveUrls(),
                 IsA<HTMLDocument>(document) ? SerializationType::kHtml
                                             : SerializationType::kXml),
      start_(start),
      end_(end),
      document_(document),
      options_(options) {}

void StyledMarkupAccumulator::AppendEndTag(const Element& element) {
  AppendEndMarkup(element, result_);
}

void StyledMarkupAccumulator::AppendStartMarkup(Node& node) {
  formatter_.AppendStartMarkup(node, result_);
}

void StyledMarkupAccumulator::AppendEndMarkup(const Element& element,
                                              StringBuilder& result) {
  formatter_.AppendEndMarkup(element, result);
}

void StyledMarkupAccumulator::AppendText(Text& text) {
  const String& str = text.data();
  wtf_size_t length = str.length();
  wtf_size_t start = 0;
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
      StringView(str, start, length), formatter_.EntityMaskForText(text),
      result_);
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
    MarkupFormatter::AppendAttributeValue(inline_style->Style()->AsText(),
                                          GetSerializationType(*document_),
                                          result_);
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
    MarkupFormatter::AppendCharactersReplacingEntities(
        content, kEntityMaskInPcdata, buffer);
    // Keep collapsible white spaces as is during markup sanitization.
    const String text_to_append =
        IsForMarkupSanitization()
            ? buffer.ToString()
            : ConvertHtmlTextToInterchangeFormat(buffer.ToString(), text);
    result_.Append(text_to_append);
  }
  if (inline_style)
    result_.Append("</span>");
}

void StyledMarkupAccumulator::AppendElementWithInlineStyle(
    const Element& element,
    const EditingStyle* style) {
  AppendElementWithInlineStyle(element, style, result_);
}

void StyledMarkupAccumulator::AppendElementWithInlineStyle(
    const Element& element,
    const EditingStyle* style,
    StringBuilder& out) {
  const SerializationType type = GetSerializationType(element.GetDocument());
  formatter_.AppendStartTagOpen(element, out);
  AttributeCollection attributes = element.Attributes();
  for (const auto& attribute : attributes) {
    // We'll handle the style attribute separately, below.
    if (attribute.GetName() == html_names::kStyleAttr)
      continue;
    AppendAttribute(element, attribute, out);
  }
  if (style && !style->IsEmpty()) {
    out.Append(" style=\"");
    MarkupFormatter::AppendAttributeValue(style->Style()->AsText(), type, out);
    out.Append('\"');
  }
  formatter_.AppendStartTagClose(element, out);
}

void StyledMarkupAccumulator::AppendElement(const Element& element) {
  AppendElement(element, result_);
}

void StyledMarkupAccumulator::AppendElement(const Element& element,
                                            StringBuilder& out) {
  formatter_.AppendStartTagOpen(element, out);
  AttributeCollection attributes = element.Attributes();
  for (const auto& attribute : attributes)
    AppendAttribute(element, attribute, out);
  formatter_.AppendStartTagClose(element, out);
}

void StyledMarkupAccumulator::AppendAttribute(const Element& element,
                                              const Attribute& attribute,
                                              StringBuilder& result) {
  String value = formatter_.ResolveUrlIfNeeded(element, attribute);
  if (formatter_.SerializeAsHtml()) {
    MarkupFormatter::AppendAttributeAsHtml(attribute, value, result);
  } else {
    MarkupFormatter::AppendAttributeAsXmlWithoutNamespace(attribute, value,
                                                          result);
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
      style->AsText(), GetSerializationType(*document_), open_tag);
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
                       .SetIgnoresCssTextTransforms(
                           options_.IgnoresCssTextTransformsForRenderedText())
                       .Build());
}

String StyledMarkupAccumulator::StringValueForRange(const Text& node) {
  if (start_.IsNull())
    return node.data();

  String str = node.data();
  if (start_.GetText() == node)
    str = str.substr(0, end_.Offset());
  if (end_.GetText() == node)
    str.erase(0, start_.Offset());
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
