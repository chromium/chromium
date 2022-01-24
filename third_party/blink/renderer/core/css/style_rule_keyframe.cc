// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"

#include <memory>
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StyleRuleKeyframe::StyleRuleKeyframe(std::unique_ptr<Vector<double>> keys,
                                     CSSPropertyValueSet* properties)
    : StyleRuleBase(kKeyframe), properties_(properties), keys_(*keys) {}

String StyleRuleKeyframe::KeyText() const {
  DCHECK(!keys_.IsEmpty());

  StringBuilder key_text;
  for (unsigned i = 0; i < keys_.size(); ++i) {
    if (i)
      key_text.Append(", ");
    key_text.AppendNumber(keys_.at(i) * 100);
    key_text.Append('%');
  }

  return key_text.ReleaseString();
}

bool StyleRuleKeyframe::SetKeyText(const String& key_text) {
  DCHECK(!key_text.IsNull());

  std::unique_ptr<Vector<double>> keys =
      CSSParser::ParseKeyframeKeyList(key_text);
  if (!keys || keys->IsEmpty())
    return false;

  keys_ = *keys;
  return true;
}

const Vector<double>& StyleRuleKeyframe::Keys() const {
  return keys_;
}

MutableCSSPropertyValueSet& StyleRuleKeyframe::MutableProperties() {
  if (!properties_->IsMutable())
    properties_ = properties_->MutableCopy();
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

String StyleRuleKeyframe::CssText() const {
  StringBuilder result;
  result.Append(KeyText());
  result.Append(" { ");
  String decls = properties_->AsText();
  result.Append(decls);
  if (!decls.IsEmpty())
    result.Append(' ');
  result.Append('}');
  return result.ReleaseString();
}

void StyleRuleKeyframe::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
