/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SCRIPT_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SCRIPT_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/create_element_flags.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/core/script/script_loader.h"

namespace blink {

class StringOrTrustedScript;
class ExceptionState;

class CORE_EXPORT HTMLScriptElement final : public HTMLElement,
                                            public ScriptElementBase {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLScriptElement);

 public:
  HTMLScriptElement(Document&, const CreateElementFlags);

  // Returns attributes that should be checked against Trusted Types
  const AttrNameToTrustedType& GetCheckedAttributeTypes() const override;

  void text(StringOrTrustedScript& result);
  void setText(const StringOrTrustedScript&, ExceptionState&);
  void setInnerText(const StringOrTrustedScript&, ExceptionState&) override;
  void setTextContent(const StringOrTrustedScript&, ExceptionState&) override;

  void setAsync(bool);
  bool async() const;

  ScriptLoader* Loader() const final { return loader_.Get(); }

  bool IsScriptElement() const override { return true; }
  Document& GetDocument() const override;

  void Trace(Visitor*) override;

 private:
  void ParseAttribute(const AttributeModificationParams&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void DidNotifySubtreeInsertionsToDocument() override;
  void ChildrenChanged(const ChildrenChange&) override;
  void DidMoveToNewDocument(Document& old_document) override;

  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& SubResourceAttributeName() const override;

  // ScriptElementBase overrides:
  String SourceAttributeValue() const override;
  String CharsetAttributeValue() const override;
  String TypeAttributeValue() const override;
  String LanguageAttributeValue() const override;
  bool NomoduleAttributeValue() const override;
  String ForAttributeValue() const override;
  String EventAttributeValue() const override;
  String CrossOriginAttributeValue() const override;
  String IntegrityAttributeValue() const override;
  String ReferrerPolicyAttributeValue() const override;
  String ImportanceAttributeValue() const override;
  String TextFromChildren() override;
  bool AsyncAttributeValue() const override;
  bool DeferAttributeValue() const override;
  bool HasSourceAttribute() const override;
  bool IsConnected() const override;
  bool HasChildren() const override;
  const AtomicString& GetNonceForElement() const override;
  bool ElementHasDuplicateAttributes() const override {
    return HasDuplicateAttribute();
  }
  bool AllowInlineScriptForCSP(const AtomicString& nonce,
                               const WTF::OrdinalNumber&,
                               const String& script_content) override;
  void DispatchLoadEvent() override;
  void DispatchErrorEvent() override;
  void SetScriptElementForBinding(
      HTMLScriptElementOrSVGScriptElement&) override;

  Element& CloneWithoutAttributesAndChildren(Document&) const override;

  Member<ScriptLoader> loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SCRIPT_ELEMENT_H_
