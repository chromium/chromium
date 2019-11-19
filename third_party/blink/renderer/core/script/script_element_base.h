/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_ELEMENT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_ELEMENT_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class Document;
class Element;
class HTMLScriptElementOrSVGScriptElement;
class ScriptLoader;

ScriptLoader* ScriptLoaderFromElement(Element*);

class CORE_EXPORT ScriptElementBase : public GarbageCollectedMixin {
 public:
  virtual bool AsyncAttributeValue() const = 0;
  virtual String CharsetAttributeValue() const = 0;
  virtual String CrossOriginAttributeValue() const = 0;
  virtual bool DeferAttributeValue() const = 0;
  virtual String EventAttributeValue() const = 0;
  virtual String ForAttributeValue() const = 0;
  virtual String IntegrityAttributeValue() const = 0;
  virtual String LanguageAttributeValue() const = 0;
  virtual bool NomoduleAttributeValue() const = 0;
  virtual String SourceAttributeValue() const = 0;
  virtual String TypeAttributeValue() const = 0;
  virtual String ReferrerPolicyAttributeValue() const = 0;
  virtual String ImportanceAttributeValue() const = 0;

  virtual String TextFromChildren() = 0;
  virtual bool HasSourceAttribute() const = 0;
  virtual bool IsConnected() const = 0;
  virtual bool HasChildren() const = 0;
  virtual const AtomicString& GetNonceForElement() const = 0;
  virtual bool ElementHasDuplicateAttributes() const = 0;

  // Whether the inline script is allowed by the CSP. Must be called
  // synchronously to ensure the correct Javascript world is used for CSP
  // checks.
  virtual bool AllowInlineScriptForCSP(const AtomicString& nonce,
                                       const WTF::OrdinalNumber&,
                                       const String& script_content) = 0;
  virtual Document& GetDocument() const = 0;
  virtual void SetScriptElementForBinding(
      HTMLScriptElementOrSVGScriptElement&) = 0;

  virtual void DispatchLoadEvent() = 0;
  virtual void DispatchErrorEvent() = 0;

 protected:
  ScriptLoader* InitializeScriptLoader(bool parser_inserted,
                                       bool already_started);

  virtual ScriptLoader* Loader() const = 0;
};

}  // namespace blink

#endif
