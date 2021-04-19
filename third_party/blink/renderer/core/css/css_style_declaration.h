/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STYLE_DECLARATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STYLE_DECLARATION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;
class CSSValue;
class ExceptionState;
enum class SecureContextMode;

class CORE_EXPORT CSSStyleDeclaration : public ScriptWrappable,
                                        public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSStyleDeclaration(const CSSStyleDeclaration&) = delete;
  CSSStyleDeclaration& operator=(const CSSStyleDeclaration&) = delete;
  ~CSSStyleDeclaration() override = default;

  void Trace(Visitor* visitor) const override;

  virtual CSSRule* parentRule() const = 0;
  String cssFloat() { return GetPropertyValueInternal(CSSPropertyID::kFloat); }
  void setCSSFloat(const ExecutionContext* execution_context,
                   const String& value,
                   ExceptionState& exception_state) {
    SetPropertyInternal(CSSPropertyID::kFloat, String(), value, false,
                        execution_context->GetSecureContextMode(),
                        exception_state);
  }
  virtual String cssText() const = 0;
  virtual void setCSSText(const ExecutionContext*,
                          const String&,
                          ExceptionState&) = 0;
  virtual unsigned length() const = 0;
  virtual String item(unsigned index) const = 0;
  virtual String getPropertyValue(const String& property_name) = 0;
  virtual String getPropertyPriority(const String& property_name) = 0;
  virtual String GetPropertyShorthand(const String& property_name) = 0;
  virtual bool IsPropertyImplicit(const String& property_name) = 0;
  virtual void setProperty(const ExecutionContext*,
                           const String& property_name,
                           const String& value,
                           const String& priority,
                           ExceptionState&) = 0;
  virtual String removeProperty(const String& property_name,
                                ExceptionState&) = 0;

  // CSSPropertyID versions of the CSSOM functions to support bindings and
  // editing.
  // Use the non-virtual methods in the concrete subclasses when possible.
  // The CSSValue returned by this function should not be exposed to the web as
  // it may be used by multiple documents at the same time.
  virtual const CSSValue* GetPropertyCSSValueInternal(CSSPropertyID) = 0;
  virtual const CSSValue* GetPropertyCSSValueInternal(
      AtomicString custom_property_name) = 0;
  virtual String GetPropertyValueInternal(CSSPropertyID) = 0;
  virtual void SetPropertyInternal(CSSPropertyID,
                                   const String& property_value,
                                   const String& value,
                                   bool important,
                                   SecureContextMode,
                                   ExceptionState&) = 0;

  virtual bool CssPropertyMatches(CSSPropertyID, const CSSValue&) const = 0;
  virtual CSSStyleSheet* ParentStyleSheet() const { return nullptr; }

  String AnonymousNamedGetter(const AtomicString& name);
  // Note: AnonymousNamedSetter() can end up throwing an exception via
  // SetPropertyInternal() even though it does not take an |ExceptionState| as
  // an argument (see bug 829408).
  NamedPropertySetterResult AnonymousNamedSetter(ScriptState*,
                                                 const AtomicString& name,
                                                 const String& value);
  NamedPropertyDeleterResult AnonymousNamedDeleter(const AtomicString& name);
  void NamedPropertyEnumerator(Vector<String>& names, ExceptionState&);
  bool NamedPropertyQuery(const AtomicString&, ExceptionState&);

 protected:
  CSSStyleDeclaration(ExecutionContext* context)
      : ExecutionContextClient(context) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STYLE_DECLARATION_H_
