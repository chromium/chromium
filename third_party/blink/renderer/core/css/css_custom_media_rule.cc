// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_custom_media_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

class MediaQuerySetOwner;

CSSCustomMediaRule::CSSCustomMediaRule(StyleRuleCustomMedia* custom_media_rule,
                                       CSSStyleSheet* parent)
    : CSSRule(parent), custom_media_rule_(custom_media_rule) {}

CSSCustomMediaRule::~CSSCustomMediaRule() = default;

String CSSCustomMediaRule::name() const {
  StringBuilder result;
  String name = custom_media_rule_->GetName();
  if (!name.empty()) {
    SerializeIdentifier(name, result);
  }
  return result.ReleaseString();
}

V8CustomMediaQuery* CSSCustomMediaRule::query() {
  if (custom_media_rule_->IsBooleanValue()) {
    return MakeGarbageCollected<V8CustomMediaQuery>(
        custom_media_rule_->GetBooleanValue());
  }
  if (custom_media_rule_->IsMediaQueryValue()) {
    return MakeGarbageCollected<V8CustomMediaQuery>(
        MakeGarbageCollected<MediaList>(this));
  }
  return nullptr;
}

void CSSCustomMediaRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  custom_media_rule_ = To<StyleRuleCustomMedia>(rule);
}

String CSSCustomMediaRule::cssText() const {
  StringBuilder result;
  result.Append("@custom-media");
  if (custom_media_rule_->IsBooleanValue()) {
    result.Append(' ');
    StringView str = custom_media_rule_->GetBooleanValue() ? "true" : "false";
    result.Append(str);
  }
  if (custom_media_rule_->IsMediaQueryValue()) {
    result.Append(' ');
    result.Append(custom_media_rule_->GetMediaQueryValue()->MediaText());
  }
  return result.ReleaseString();
}

const MediaQuerySet* CSSCustomMediaRule::MediaQueries() const {
  if (custom_media_rule_->IsMediaQueryValue()) {
    return custom_media_rule_->GetMediaQueryValue();
  }
  return nullptr;
}

void CSSCustomMediaRule::SetMediaQueries(const MediaQuerySet* media_queries) {
  custom_media_rule_->SetMediaQueries(media_queries);
}

void CSSCustomMediaRule::Trace(Visitor* visitor) const {
  visitor->Trace(custom_media_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
