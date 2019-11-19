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

class HTMLStackItem final : public GarbageCollected<HTMLStackItem> {
 public:
  enum ItemType { kItemForContextElement, kItemForDocumentFragmentNode };

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
      const AtomicString& namespace_uri = html_names::xhtmlNamespaceURI)
      : node_(node),
        token_local_name_(token->GetName()),
        token_attributes_(token->Attributes()),
        namespace_uri_(namespace_uri),
        is_document_fragment_node_(false) {}

  Element* GetElement() const { return To<Element>(node_.Get()); }
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
           namespace_uri_ == html_names::xhtmlNamespaceURI;
  }
  bool MatchesHTMLTag(const QualifiedName& name) const {
    return token_local_name_ == name &&
           namespace_uri_ == html_names::xhtmlNamespaceURI;
  }

  bool CausesFosterParenting() {
    return HasTagName(html_names::kTableTag) ||
           HasTagName(html_names::kTbodyTag) ||
           HasTagName(html_names::kTfootTag) ||
           HasTagName(html_names::kTheadTag) || HasTagName(html_names::kTrTag);
  }

  bool IsInHTMLNamespace() const {
    // A DocumentFragment takes the place of the document element when parsing
    // fragments and should be considered in the HTML namespace.
    return NamespaceURI() == html_names::xhtmlNamespaceURI ||
           IsDocumentFragmentNode();  // FIXME: Does this also apply to
                                      // ShadowRoot?
  }

  bool IsNumberedHeaderElement() const {
    return HasTagName(html_names::kH1Tag) || HasTagName(html_names::kH2Tag) ||
           HasTagName(html_names::kH3Tag) || HasTagName(html_names::kH4Tag) ||
           HasTagName(html_names::kH5Tag) || HasTagName(html_names::kH6Tag);
  }

  bool IsTableBodyContextElement() const {
    return HasTagName(html_names::kTbodyTag) ||
           HasTagName(html_names::kTfootTag) ||
           HasTagName(html_names::kTheadTag);
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
    return tag_name == html_names::kAddressTag ||
           tag_name == html_names::kAreaTag ||
           tag_name == html_names::kAppletTag ||
           tag_name == html_names::kArticleTag ||
           tag_name == html_names::kAsideTag ||
           tag_name == html_names::kBaseTag ||
           tag_name == html_names::kBasefontTag ||
           tag_name == html_names::kBgsoundTag ||
           tag_name == html_names::kBlockquoteTag ||
           tag_name == html_names::kBodyTag || tag_name == html_names::kBrTag ||
           tag_name == html_names::kButtonTag ||
           tag_name == html_names::kCaptionTag ||
           tag_name == html_names::kCenterTag ||
           tag_name == html_names::kColTag ||
           tag_name == html_names::kColgroupTag ||
           tag_name == html_names::kCommandTag ||
           tag_name == html_names::kDdTag ||
           tag_name == html_names::kDetailsTag ||
           tag_name == html_names::kDirTag || tag_name == html_names::kDivTag ||
           tag_name == html_names::kDlTag || tag_name == html_names::kDtTag ||
           tag_name == html_names::kEmbedTag ||
           tag_name == html_names::kFieldsetTag ||
           tag_name == html_names::kFigcaptionTag ||
           tag_name == html_names::kFigureTag ||
           tag_name == html_names::kFooterTag ||
           tag_name == html_names::kFormTag ||
           tag_name == html_names::kFrameTag ||
           tag_name == html_names::kFramesetTag || IsNumberedHeaderElement() ||
           tag_name == html_names::kHeadTag ||
           tag_name == html_names::kHeaderTag ||
           tag_name == html_names::kHgroupTag ||
           tag_name == html_names::kHrTag || tag_name == html_names::kHTMLTag ||
           tag_name == html_names::kIFrameTag ||
           tag_name == html_names::kImgTag ||
           tag_name == html_names::kInputTag ||
           tag_name == html_names::kLiTag || tag_name == html_names::kLinkTag ||
           tag_name == html_names::kListingTag ||
           tag_name == html_names::kMainTag ||
           tag_name == html_names::kMarqueeTag ||
           tag_name == html_names::kMenuTag ||
           tag_name == html_names::kMetaTag ||
           tag_name == html_names::kNavTag ||
           tag_name == html_names::kNoembedTag ||
           tag_name == html_names::kNoframesTag ||
           tag_name == html_names::kNoscriptTag ||
           tag_name == html_names::kObjectTag ||
           tag_name == html_names::kOlTag || tag_name == html_names::kPTag ||
           tag_name == html_names::kParamTag ||
           tag_name == html_names::kPlaintextTag ||
           tag_name == html_names::kPreTag ||
           tag_name == html_names::kScriptTag ||
           tag_name == html_names::kSectionTag ||
           tag_name == html_names::kSelectTag ||
           tag_name == html_names::kStyleTag ||
           tag_name == html_names::kSummaryTag ||
           tag_name == html_names::kTableTag || IsTableBodyContextElement() ||
           tag_name == html_names::kTdTag ||
           tag_name == html_names::kTemplateTag ||
           tag_name == html_names::kTextareaTag ||
           tag_name == html_names::kThTag ||
           tag_name == html_names::kTitleTag ||
           tag_name == html_names::kTrTag || tag_name == html_names::kUlTag ||
           tag_name == html_names::kWbrTag || tag_name == html_names::kXmpTag;
  }

  void Trace(Visitor* visitor) { visitor->Trace(node_); }

 private:
  Member<ContainerNode> node_;

  AtomicString token_local_name_;
  Vector<Attribute> token_attributes_;
  AtomicString namespace_uri_;
  bool is_document_fragment_node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_STACK_ITEM_H_
