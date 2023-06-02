/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xml/parser/xml_errors.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xml/document_xslt.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

const int kMaxErrors = 25;

XMLErrors::XMLErrors(Document* document)
    : document_(document),
      error_count_(0),
      last_error_position_(TextPosition::BelowRangePosition()) {}

void XMLErrors::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

void XMLErrors::HandleError(ErrorType type,
                            const char* message,
                            int line_number,
                            int column_number) {
  HandleError(type, message,
              TextPosition(OrdinalNumber::FromOneBasedInt(line_number),
                           OrdinalNumber::FromOneBasedInt(column_number)));
}

void XMLErrors::HandleError(ErrorType type,
                            const char* message,
                            TextPosition position) {
  if (type == kErrorTypeFatal ||
      (error_count_ < kMaxErrors &&
       last_error_position_.line_ != position.line_ &&
       last_error_position_.column_ != position.column_)) {
    switch (type) {
      case kErrorTypeWarning:
        AppendErrorMessage("warning", position, message);
        break;
      case kErrorTypeFatal:
      case kErrorTypeNonFatal:
        AppendErrorMessage("error", position, message);
    }

    last_error_position_ = position;
    ++error_count_;
  }
}

void XMLErrors::AppendErrorMessage(const String& type_string,
                                   TextPosition position,
                                   const char* message) {
  // <typeString> on line <lineNumber> at column <columnNumber>: <message>
  error_messages_.Append(type_string);
  error_messages_.Append(" on line ");
  error_messages_.AppendNumber(position.line_.OneBasedInt());
  error_messages_.Append(" at column ");
  error_messages_.AppendNumber(position.column_.OneBasedInt());
  error_messages_.Append(": ");
  error_messages_.Append(message);
}

static inline Element* CreateXHTMLParserErrorHeader(
    Document* doc,
    const String& error_messages) {
  const CreateElementFlags flags = CreateElementFlags::ByParser(doc);
  Element* report_element = doc->CreateRawElement(
      QualifiedName(g_null_atom, AtomicString("parsererror"),
                    html_names::xhtmlNamespaceURI),
      flags);

  Vector<Attribute, kAttributePrealloc> report_attributes;
  report_attributes.push_back(Attribute(
      html_names::kStyleAttr,
      AtomicString(
          "display: block; white-space: pre; border: 2px solid #c77; padding: "
          "0 1em 0 1em; margin: 1em; background-color: #fdd; color: black")));
  report_element->ParserSetAttributes(report_attributes);

  Element* h3 = doc->CreateRawElement(html_names::kH3Tag, flags);
  report_element->ParserAppendChild(h3);
  h3->ParserAppendChild(
      doc->createTextNode("This page contains the following errors:"));

  Element* fixed = doc->CreateRawElement(html_names::kDivTag, flags);
  Vector<Attribute, kAttributePrealloc> fixed_attributes;
  fixed_attributes.push_back(
      Attribute(html_names::kStyleAttr,
                AtomicString("font-family:monospace;font-size:12px")));
  fixed->ParserSetAttributes(fixed_attributes);
  report_element->ParserAppendChild(fixed);

  fixed->ParserAppendChild(doc->createTextNode(error_messages));

  h3 = doc->CreateRawElement(html_names::kH3Tag, flags);
  report_element->ParserAppendChild(h3);
  h3->ParserAppendChild(doc->createTextNode(
      "Below is a rendering of the page up to the first error."));

  return report_element;
}

void XMLErrors::InsertErrorMessageBlock() {
  // One or more errors occurred during parsing of the code. Display an error
  // block to the user above the normal content (the DOM tree is created
  // manually and includes line/col info regarding where the errors are located)

  // Create elements for display
  const CreateElementFlags flags = CreateElementFlags::ByParser(document_);
  Element* document_element = document_->documentElement();
  if (!document_element) {
    Element* root_element =
        document_->CreateRawElement(html_names::kHTMLTag, flags);
    Element* body = document_->CreateRawElement(html_names::kBodyTag, flags);
    root_element->ParserAppendChild(body);
    document_->ParserAppendChild(root_element);
    document_element = body;
  } else if (document_element->namespaceURI() == svg_names::kNamespaceURI) {
    Element* root_element =
        document_->CreateRawElement(html_names::kHTMLTag, flags);
    Element* head = document_->CreateRawElement(html_names::kHeadTag, flags);
    Element* style = document_->CreateRawElement(html_names::kStyleTag, flags);
    head->ParserAppendChild(style);
    style->ParserAppendChild(
        document_->createTextNode("html, body { height: 100% } parsererror + "
                                  "svg { width: 100%; height: 100% }"));
    style->FinishParsingChildren();
    root_element->ParserAppendChild(head);
    Element* body = document_->CreateRawElement(html_names::kBodyTag, flags);
    root_element->ParserAppendChild(body);

    document_->ParserRemoveChild(*document_element);

    body->ParserAppendChild(document_element);
    document_->ParserAppendChild(root_element);

    document_element = body;
  }

  String error_messages = error_messages_.ToString();
  Element* report_element =
      CreateXHTMLParserErrorHeader(document_, error_messages);

  if (DocumentXSLT::HasTransformSourceDocument(*document_)) {
    Vector<Attribute, kAttributePrealloc> attributes;
    attributes.push_back(
        Attribute(html_names::kStyleAttr, AtomicString("white-space: normal")));
    Element* paragraph = document_->CreateRawElement(html_names::kPTag, flags);
    paragraph->ParserSetAttributes(attributes);
    paragraph->ParserAppendChild(document_->createTextNode(
        "This document was created as the result of an XSL transformation. The "
        "line and column numbers given are from the transformed result."));
    report_element->ParserAppendChild(paragraph);
  }

  Node* first_child = document_element->firstChild();
  if (first_child)
    document_element->ParserInsertBefore(report_element, *first_child);
  else
    document_element->ParserAppendChild(report_element);

  // FIXME: Why do we need to call this manually?
  document_->UpdateStyleAndLayoutTree();
}

}  // namespace blink
