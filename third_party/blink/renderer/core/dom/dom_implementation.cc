/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#include "third_party/blink/renderer/core/dom/dom_implementation.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html/text_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

DOMImplementation::DOMImplementation(Document& document)
    : document_(document) {}

DocumentType* DOMImplementation::createDocumentType(
    const AtomicString& qualified_name,
    const String& public_id,
    const String& system_id,
    ExceptionState& exception_state) {
  AtomicString prefix, local_name;
  if (!Document::ParseQualifiedName(qualified_name, prefix, local_name,
                                    exception_state))
    return nullptr;
  if (!document_->GetExecutionContext())
    return nullptr;

  return MakeGarbageCollected<DocumentType>(document_, qualified_name,
                                            public_id, system_id);
}

XMLDocument* DOMImplementation::createDocument(
    const AtomicString& namespace_uri,
    const AtomicString& qualified_name,
    DocumentType* doctype,
    ExceptionState& exception_state) {
  XMLDocument* doc = nullptr;
  ExecutionContext* context = document_->GetExecutionContext();
  DocumentInit init =
      DocumentInit::Create().WithExecutionContext(context).WithAgent(
          document_->GetAgent());
  if (namespace_uri == svg_names::kNamespaceURI) {
    doc = XMLDocument::CreateSVG(init);
  } else if (namespace_uri == html_names::xhtmlNamespaceURI) {
    doc = XMLDocument::CreateXHTML(init);
  } else {
    doc = MakeGarbageCollected<XMLDocument>(init);
  }

  Node* document_element = nullptr;
  if (!qualified_name.empty()) {
    document_element =
        doc->createElementNS(namespace_uri, qualified_name, exception_state);
    if (exception_state.HadException())
      return nullptr;
  }

  if (doctype)
    doc->AppendChild(doctype);
  if (document_element)
    doc->AppendChild(document_element);

  return doc;
}

Document* DOMImplementation::createHTMLDocument(const String& title) {
  DocumentInit init =
      DocumentInit::Create()
          .WithExecutionContext(document_->GetExecutionContext())
          .WithAgent(document_->GetAgent());
  auto* d = MakeGarbageCollected<HTMLDocument>(init);
  d->setAllowDeclarativeShadowRoots(false);
  d->open();
  d->write("<!doctype html><html><head></head><body></body></html>");
  if (!title.IsNull()) {
    HTMLHeadElement* head_element = d->head();
    DCHECK(head_element);
    auto* title_element = MakeGarbageCollected<HTMLTitleElement>(*d);
    head_element->AppendChild(title_element);
    title_element->AppendChild(d->createTextNode(title), ASSERT_NO_EXCEPTION);
  }
  return d;
}

void DOMImplementation::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
