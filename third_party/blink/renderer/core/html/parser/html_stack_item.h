/*
 * Copyright (C) 2012 Company 100, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_STACK_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_STACK_ITEM_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ContainerNode;

class HTMLStackItem : public GarbageCollectedFinalized<HTMLStackItem> {
 public:
  enum ItemType { kItemForContextElement, kItemForDocumentFragmentNode };

  // Used by document fragment node and context element.
  static HTMLStackItem* Create(ContainerNode* node, ItemType type) {
    return new HTMLStackItem(node, type);
  }

  // Used by HTMLElementStack and HTMLFormattingElementList.
  static HTMLStackItem* Create(
      ContainerNode* node,
      AtomicHTMLToken* token,
      const AtomicString& namespace_uri = HTMLNames::xhtmlNamespaceURI) {
    return new HTMLStackItem(node, token, namespace_uri);
  }

  Element* GetElement() const { return ToElement(node_.Get()); }
  ContainerNode* GetNode() const { return node_.Get(); }

  bool IsDocumentFragmentNode() const { return is_document_fragment_node_; }
  bool IsElementNode() const { return !is_document_fragment_node_; }

  const AtomicString& NamespaceURI() const { return namespace_uri_; }
  const AtomicString& LocalName() const { return token_local_name_; }

  const Vector<Attribute>& Attributes() const {
    DCHECK(token_local_name_);
    return token_attributes_;
  }
  Attribute* GetAttributeItem(const QualifiedName& attribute_name) {
    DCHECK(token_local_name_);
    return FindAttributeInVector(token_attributes_, attribute_name);
  }

  bool HasLocalName(const AtomicString& name) const {
    return token_local_name_ == name;
  }
  bool HasTagName(const QualifiedName& name) const {
    return token_local_name_ == name.LocalName() &&
           namespace_uri_ == name.NamespaceURI();
  }

  bool MatchesHTMLTag(const AtomicString& name) const {
    return token_local_name_ == name &&
           namespace_uri_ == HTMLNames::xhtmlNamespaceURI;
  }
  bool MatchesHTMLTag(const QualifiedName& name) const {
    return token_local_name_ == name &&
           namespace_uri_ == HTMLNames::xhtmlNamespaceURI;
  }

  bool CausesFosterParenting() {
    return HasTagName(HTMLNames::tableTag) || HasTagName(HTMLNames::tbodyTag) ||
           HasTagName(HTMLNames::tfootTag) || HasTagName(HTMLNames::theadTag) ||
           HasTagName(HTMLNames::trTag);
  }

  bool IsInHTMLNamespace() const {
    // A DocumentFragment takes the place of the document element when parsing
    // fragments and should be considered in the HTML namespace.
    return NamespaceURI() == HTMLNames::xhtmlNamespaceURI ||
           IsDocumentFragmentNode();  // FIXME: Does this also apply to
                                      // ShadowRoot?
  }

  bool IsNumberedHeaderElement() const {
    return HasTagName(HTMLNames::h1Tag) || HasTagName(HTMLNames::h2Tag) ||
           HasTagName(HTMLNames::h3Tag) || HasTagName(HTMLNames::h4Tag) ||
           HasTagName(HTMLNames::h5Tag) || HasTagName(HTMLNames::h6Tag);
  }

  bool IsTableBodyContextElement() const {
    return HasTagName(HTMLNames::tbodyTag) || HasTagName(HTMLNames::tfootTag) ||
           HasTagName(HTMLNames::theadTag);
  }

  // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#special
  bool IsSpecialNode() const {
    if (HasTagName(mathml_names::kMiTag) || HasTagName(mathml_names::kMoTag) ||
        HasTagName(mathml_names::kMnTag) || HasTagName(mathml_names::kMsTag) ||
        HasTagName(mathml_names::kMtextTag) ||
        HasTagName(mathml_names::kAnnotationXmlTag) ||
        HasTagName(svg_names::kForeignObjectTag) ||
        HasTagName(svg_names::kDescTag) || HasTagName(svg_names::kTitleTag))
      return true;
    if (IsDocumentFragmentNode())
      return true;
    if (!IsInHTMLNamespace())
      return false;
    const AtomicString& tag_name = LocalName();
    return tag_name == HTMLNames::addressTag ||
           tag_name == HTMLNames::areaTag || tag_name == HTMLNames::appletTag ||
           tag_name == HTMLNames::articleTag ||
           tag_name == HTMLNames::asideTag || tag_name == HTMLNames::baseTag ||
           tag_name == HTMLNames::basefontTag ||
           tag_name == HTMLNames::bgsoundTag ||
           tag_name == HTMLNames::blockquoteTag ||
           tag_name == HTMLNames::bodyTag || tag_name == HTMLNames::brTag ||
           tag_name == HTMLNames::buttonTag ||
           tag_name == HTMLNames::captionTag ||
           tag_name == HTMLNames::centerTag || tag_name == HTMLNames::colTag ||
           tag_name == HTMLNames::colgroupTag ||
           tag_name == HTMLNames::commandTag || tag_name == HTMLNames::ddTag ||
           tag_name == HTMLNames::detailsTag || tag_name == HTMLNames::dirTag ||
           tag_name == HTMLNames::divTag || tag_name == HTMLNames::dlTag ||
           tag_name == HTMLNames::dtTag || tag_name == HTMLNames::embedTag ||
           tag_name == HTMLNames::fieldsetTag ||
           tag_name == HTMLNames::figcaptionTag ||
           tag_name == HTMLNames::figureTag ||
           tag_name == HTMLNames::footerTag || tag_name == HTMLNames::formTag ||
           tag_name == HTMLNames::frameTag ||
           tag_name == HTMLNames::framesetTag || IsNumberedHeaderElement() ||
           tag_name == HTMLNames::headTag || tag_name == HTMLNames::headerTag ||
           tag_name == HTMLNames::hgroupTag || tag_name == HTMLNames::hrTag ||
           tag_name == HTMLNames::htmlTag || tag_name == HTMLNames::iframeTag ||
           tag_name == HTMLNames::imgTag || tag_name == HTMLNames::inputTag ||
           tag_name == HTMLNames::liTag || tag_name == HTMLNames::linkTag ||
           tag_name == HTMLNames::listingTag ||
           tag_name == HTMLNames::mainTag ||
           tag_name == HTMLNames::marqueeTag ||
           tag_name == HTMLNames::menuTag ||
           tag_name == HTMLNames::metaTag || tag_name == HTMLNames::navTag ||
           tag_name == HTMLNames::noembedTag ||
           tag_name == HTMLNames::noframesTag ||
           tag_name == HTMLNames::noscriptTag ||
           tag_name == HTMLNames::objectTag || tag_name == HTMLNames::olTag ||
           tag_name == HTMLNames::pTag || tag_name == HTMLNames::paramTag ||
           tag_name == HTMLNames::plaintextTag ||
           tag_name == HTMLNames::preTag || tag_name == HTMLNames::scriptTag ||
           tag_name == HTMLNames::sectionTag ||
           tag_name == HTMLNames::selectTag ||
           tag_name == HTMLNames::styleTag ||
           tag_name == HTMLNames::summaryTag ||
           tag_name == HTMLNames::tableTag || IsTableBodyContextElement() ||
           tag_name == HTMLNames::tdTag || tag_name == HTMLNames::templateTag ||
           tag_name == HTMLNames::textareaTag || tag_name == HTMLNames::thTag ||
           tag_name == HTMLNames::titleTag || tag_name == HTMLNames::trTag ||
           tag_name == HTMLNames::ulTag || tag_name == HTMLNames::wbrTag ||
           tag_name == HTMLNames::xmpTag;
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(node_); }

 private:
  HTMLStackItem(ContainerNode* node, ItemType type) : node_(node) {
    switch (type) {
      case kItemForDocumentFragmentNode:
        is_document_fragment_node_ = true;
        break;
      case kItemForContextElement:
        token_local_name_ = GetElement()->localName();
        namespace_uri_ = GetElement()->namespaceURI();
        is_document_fragment_node_ = false;
        break;
    }
  }

  HTMLStackItem(
      ContainerNode* node,
      AtomicHTMLToken* token,
      const AtomicString& namespace_uri = HTMLNames::xhtmlNamespaceURI)
      : node_(node),
        token_local_name_(token->GetName()),
        token_attributes_(token->Attributes()),
        namespace_uri_(namespace_uri),
        is_document_fragment_node_(false) {}

  Member<ContainerNode> node_;

  AtomicString token_local_name_;
  Vector<Attribute> token_attributes_;
  AtomicString namespace_uri_;
  bool is_document_fragment_node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_STACK_ITEM_H_
