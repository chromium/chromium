// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_private_rule.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPrivateRule::CSSPrivateRule(StyleRule* private_rule, CSSStyleSheet* parent)
    : CSSStyleRule(private_rule, parent, /*position_hint=*/0) {
  CHECK(parent);
  property_registry_ = parent->Contents()->GetPrivatePropertyRegistry();
  CHECK(property_registry_);
  UpdatePrivateNames();
}

String CSSPrivateRule::cssText() const {
  StringBuilder result;
  result.Append("@private");
  AppendBlockText(result, property_registry_.Get());
  return result.ReleaseString();
}

void CSSPrivateRule::Reattach(StyleRuleBase* rule) {
  CSSStyleRule::Reattach(rule);
  if (parentStyleSheet()) {
    property_registry_ =
        parentStyleSheet()->Contents()->GetPrivatePropertyRegistry();
  }
  UpdatePrivateNames();
}

AtomicString CSSPrivateRule::HygienicToAuthoredCustomPropertyName(
    const AtomicString& name) const {
  const AtomicString& authored_name =
      property_registry_->AuthoredPrivatePropertyName(name);
  return authored_name.IsNull() ? name : authored_name;
}

AtomicString CSSPrivateRule::AuthoredToHygienicCustomPropertyName(
    const AtomicString& name) const {
  if (!property_registry_->AuthoredPrivatePropertyName(name).IsNull()) {
    return name;
  }
  auto it = authored_to_hygienic_names_.find(name);
  return it == authored_to_hygienic_names_.end() ? name : it->value;
}

AtomicString CSSPrivateRule::EnsureCustomPropertyNameForHygienic(
    const AtomicString& name) {
  AtomicString hygienic_name = AuthoredToHygienicCustomPropertyName(name);
  if (hygienic_name != name ||
      !property_registry_->AuthoredPrivatePropertyName(name).IsNull()) {
    return hygienic_name;
  }
  hygienic_name = PropertyRegistry::AllocatePrivatePropertyName();
  property_registry_->RegisterPrivateProperty(
      name, hygienic_name, CSSSyntaxDefinition::CreateUniversal());
  authored_to_hygienic_names_.Set(name, hygienic_name);
  return hygienic_name;
}

void CSSPrivateRule::UpdatePrivateNames() {
  for (const CSSPropertyValue& property :
       GetStyleRule()->Properties().Properties()) {
    if (property.PropertyID() != CSSPropertyID::kVariable) {
      continue;
    }
    const AtomicString& hygienic_name = property.CustomPropertyName();
    const AtomicString& authored_name =
        property_registry_->AuthoredPrivatePropertyName(hygienic_name);
    authored_to_hygienic_names_.Set(authored_name, hygienic_name);
  }
}

void CSSPrivateRule::Trace(Visitor* visitor) const {
  visitor->Trace(property_registry_);
  CSSStyleRule::Trace(visitor);
}

}  // namespace blink
