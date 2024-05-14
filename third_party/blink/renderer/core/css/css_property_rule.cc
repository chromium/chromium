// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_rule.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPropertyRule::CSSPropertyRule(StyleRuleProperty* property_rule,
                                 CSSStyleSheet* sheet)
    : CSSRule(sheet), property_rule_(property_rule) {}

CSSPropertyRule::~CSSPropertyRule() = default;

String CSSPropertyRule::cssText() const {
  // https://drafts.css-houdini.org/css-properties-values-api-1/#serialize-a-csspropertyrule
  StringBuilder builder;
  builder.Append("@property ");
  SerializeIdentifier(property_rule_->GetName(), builder);
  builder.Append(" { ");
  if (const CSSValue* syntax = property_rule_->GetSyntax()) {
    DCHECK(syntax->IsStringValue());
    builder.Append("syntax: ");
    builder.Append(syntax->CssText());
    builder.Append("; ");
  }
  if (const CSSValue* inherits = property_rule_->Inherits()) {
    DCHECK(*inherits == *CSSIdentifierValue::Create(CSSValueID::kTrue) ||
           *inherits == *CSSIdentifierValue::Create(CSSValueID::kFalse));
    builder.Append("inherits: ");
    builder.Append(inherits->CssText());
    builder.Append("; ");
  }
  if (const CSSValue* initial = property_rule_->GetInitialValue()) {
    builder.Append("initial-value: ");
    builder.Append(initial->CssText());
    builder.Append("; ");
  }
  builder.Append("}");
  return builder.ReleaseString();
}

void CSSPropertyRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  property_rule_ = To<StyleRuleProperty>(rule);
}

StyleRuleProperty* CSSPropertyRule::Property() const {
  return property_rule_.Get();
}

bool CSSPropertyRule::SetNameText(const ExecutionContext* execution_context,
                                  const String& name_text) {
  CSSStyleSheet::RuleMutationScope rule_mutation_scope(this);
  if (parentStyleSheet()) {
    parentStyleSheet()->Contents()->NotifyDiffUnrepresentable();
  }

  return property_rule_->SetNameText(execution_context, name_text);
}

String CSSPropertyRule::name() const {
  return property_rule_->GetName();
}

String CSSPropertyRule::syntax() const {
  if (const CSSValue* syntax = property_rule_->GetSyntax()) {
    return To<CSSStringValue>(*syntax).Value();
  }
  return g_null_atom;
}

bool CSSPropertyRule::inherits() const {
  if (const CSSValue* inherits = property_rule_->Inherits()) {
    switch (To<CSSIdentifierValue>(*inherits).GetValueID()) {
      case CSSValueID::kTrue:
        return true;
      case CSSValueID::kFalse:
        return false;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  return false;
}

String CSSPropertyRule::initialValue() const {
  if (const CSSValue* initial = property_rule_->GetInitialValue()) {
    return initial->CssText();
  }
  return g_null_atom;
}

CSSStyleDeclaration* CSSPropertyRule::Style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ =
        MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
            property_rule_->MutableProperties(),
            const_cast<CSSPropertyRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

void CSSPropertyRule::Trace(Visitor* visitor) const {
  visitor->Trace(property_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
