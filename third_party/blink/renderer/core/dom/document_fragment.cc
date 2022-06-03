/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/document_fragment.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

DocumentFragment::DocumentFragment(Document* document,
                                   ConstructionType construction_type)
    : ContainerNode(document, construction_type) {}

DocumentFragment* DocumentFragment::Create(Document& document) {
  return MakeGarbageCollected<DocumentFragment>(&document,
                                                Node::kCreateDocumentFragment);
}

String DocumentFragment::nodeName() const {
  return "#document-fragment";
}

Node::NodeType DocumentFragment::getNodeType() const {
  return kDocumentFragmentNode;
}

bool DocumentFragment::ChildTypeAllowed(NodeType type) const {
  switch (type) {
    case kElementNode:
    case kProcessingInstructionNode:
    case kCommentNode:
    case kTextNode:
    case kCdataSectionNode:
      return true;
    default:
      return false;
  }
}

Node* DocumentFragment::Clone(Document& factory, CloneChildrenFlag flag) const {
  DocumentFragment* clone = Create(factory);
  if (flag != CloneChildrenFlag::kSkip)
    clone->CloneChildNodesFrom(*this, flag);
  return clone;
}

void DocumentFragment::ParseHTML(const String& source,
                                 Element* context_element,
                                 ParserContentPolicy parser_content_policy) {
  RUNTIME_CALL_TIMER_SCOPE(
      V8PerIsolateData::MainThreadIsolate(),
      RuntimeCallStats::CounterId::kDocumentFragmentParseHTML);
  HTMLDocumentParser::ParseDocumentFragment(source, this, context_element,
                                            parser_content_policy);
}

bool DocumentFragment::ParseXML(const String& source,
                                Element* context_element,
                                ParserContentPolicy parser_content_policy) {
  return XMLDocumentParser::ParseDocumentFragment(source, this, context_element,
                                                  parser_content_policy);
}

}  // namespace blink
