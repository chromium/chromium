/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple, Inc. All rights reserved.
 * Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/xml/xslt_processor.h"

#include "third_party/blink/renderer/core/dom/document_encoding_data.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/xml/document_xslt.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

static inline void TransformTextStringToXHTMLDocumentString(String& text) {
  // Modify the output so that it is a well-formed XHTML document with a <pre>
  // tag enclosing the text.
  text.Replace('&', "&amp;");
  text.Replace('<', "&lt;");
  text =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
      "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
      "<head><title/></head>\n"
      "<body>\n"
      "<pre>" +
      text +
      "</pre>\n"
      "</body>\n"
      "</html>\n";
}

XSLTProcessor::~XSLTProcessor() = default;

Document* XSLTProcessor::CreateDocumentFromSource(
    const String& source_string,
    const String& source_encoding,
    const String& source_mime_type,
    Node* source_node,
    LocalFrame* frame) {
  if (!source_node->GetExecutionContext())
    return nullptr;

  KURL url = NullURL();
  Document* owner_document = &source_node->GetDocument();
  if (owner_document == source_node)
    url = owner_document->Url();
  String document_source = source_string;

  String mime_type = source_mime_type;
  // Force text/plain to be parsed as XHTML. This was added without explanation
  // in 2005:
  // https://chromium.googlesource.com/chromium/src/+/e20d8de86f154892d94798bbd8b65720a11d6299
  // It's unclear whether it's still needed for compat.
  if (source_mime_type == "text/plain") {
    mime_type = "application/xhtml+xml";
    TransformTextStringToXHTMLDocumentString(document_source);
  }

  if (frame) {
    auto* previous_document_loader = frame->Loader().GetDocumentLoader();
    DCHECK(previous_document_loader);
    std::unique_ptr<WebNavigationParams> params =
        previous_document_loader->CreateWebNavigationParamsToCloneDocument();
    WebNavigationParams::FillStaticResponse(
        params.get(), mime_type,
        source_encoding.empty() ? "UTF-8" : source_encoding,
        StringUTF8Adaptor(document_source));
    params->frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
    frame->Loader().CommitNavigation(std::move(params), nullptr,
                                     CommitReason::kXSLT);
    return frame->GetDocument();
  }

  DocumentInit init =
      DocumentInit::Create()
          .WithURL(url)
          .WithTypeFrom(mime_type)
          .WithExecutionContext(owner_document->GetExecutionContext())
          .WithAgent(owner_document->GetAgent());
  Document* document = init.CreateDocument();
  auto parsed_source_encoding = source_encoding.empty()
                                    ? UTF8Encoding()
                                    : WTF::TextEncoding(source_encoding);
  if (parsed_source_encoding.IsValid()) {
    DocumentEncodingData data;
    data.SetEncoding(parsed_source_encoding);
    document->SetEncodingData(data);
  } else {
    document_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kXml,
        mojom::blink::ConsoleMessageLevel::kWarning,
        String("Document encoding not valid: ") + source_encoding));
  }
  document->SetContent(document_source);
  return document;
}

Document* XSLTProcessor::transformToDocument(Node* source_node) {
  String result_mime_type;
  String result_string;
  String result_encoding;
  if (!TransformToString(source_node, result_mime_type, result_string,
                         result_encoding))
    return nullptr;
  return CreateDocumentFromSource(result_string, result_encoding,
                                  result_mime_type, source_node, nullptr);
}

DocumentFragment* XSLTProcessor::transformToFragment(Node* source_node,
                                                     Document* output_doc) {
  String result_mime_type;
  String result_string;
  String result_encoding;

  // If the output document is HTML, default to HTML method.
  if (IsA<HTMLDocument>(output_doc))
    result_mime_type = "text/html";

  if (!TransformToString(source_node, result_mime_type, result_string,
                         result_encoding))
    return nullptr;
  return CreateFragmentForTransformToFragment(result_string, result_mime_type,
                                              *output_doc);
}

void XSLTProcessor::setParameter(const String& /*namespaceURI*/,
                                 const String& local_name,
                                 const String& value) {
  // FIXME: namespace support?
  // should make a QualifiedName here but we'd have to expose the impl
  parameters_.Set(local_name, value);
}

String XSLTProcessor::getParameter(const String& /*namespaceURI*/,
                                   const String& local_name) const {
  // FIXME: namespace support?
  // should make a QualifiedName here but we'd have to expose the impl
  auto it = parameters_.find(local_name);
  if (it == parameters_.end())
    return String();
  return it->value;
}

void XSLTProcessor::removeParameter(const String& /*namespaceURI*/,
                                    const String& local_name) {
  // FIXME: namespace support?
  parameters_.erase(local_name);
}

void XSLTProcessor::reset() {
  stylesheet_.Clear();
  stylesheet_root_node_.Clear();
  parameters_.clear();
}

void XSLTProcessor::Trace(Visitor* visitor) const {
  visitor->Trace(stylesheet_);
  visitor->Trace(stylesheet_root_node_);
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
