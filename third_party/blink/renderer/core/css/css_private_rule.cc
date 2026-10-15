// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_private_rule.h"

#include "third_party/blink/renderer/core/css/css_function_rule.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPrivateRule::CSSPrivateRule(StyleRule* private_rule, CSSStyleSheet* parent)
    : CSSRule(parent), private_rule_(private_rule) {
  CHECK(parent);
  property_registry_ = parent->Contents()->GetPrivatePropertyRegistry();
  CHECK(property_registry_);
}

String CSSPrivateRule::cssText() const {
  StringBuilder result;
  result.Append("@private { ");
  for (const CSSPropertyValue& property :
       private_rule_->Properties().Properties()) {
    if (property.PropertyID() != CSSPropertyID::kVariable) {
      continue;
    }
    const AtomicString& private_name = property.CustomPropertyName();
    const AtomicString& authored_name =
        property_registry_->AuthoredPrivatePropertyName(private_name);
    CHECK(!authored_name.IsNull());
    const PropertyRegistration* registration =
        property_registry_->PrivateRegistration(private_name);
    CHECK(registration);
    SerializeIdentifier(authored_name, result);
    if (!registration->Syntax().IsUniversal()) {
      result.Append(' ');
      AppendCSSType(registration->Syntax(), result);
    }
    result.Append(": ");
    result.Append(property.Value().CssText());
    result.Append("; ");
  }
  result.Append('}');
  return result.ReleaseString();
}

void CSSPrivateRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  private_rule_ = To<StyleRule>(rule);
}

void CSSPrivateRule::Trace(Visitor* visitor) const {
  visitor->Trace(private_rule_);
  visitor->Trace(property_registry_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
