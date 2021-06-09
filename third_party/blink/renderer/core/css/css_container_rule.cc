// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_rule.h"

#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSContainerRule::CSSContainerRule(StyleRuleContainer* container_rule,
                                   CSSStyleSheet* parent)
    : CSSConditionRule(container_rule, parent) {}

CSSContainerRule::~CSSContainerRule() = default;

String CSSContainerRule::cssText() const {
  StringBuilder result;
  result.Append("@container ");
  if (!Name().IsNull()) {
    result.Append(Name());
    result.Append(' ');
  }
  if (ContainerQueries()) {
    result.Append(ContainerQueries()->MediaText());
    result.Append(' ');
  }
  result.Append("{\n");
  AppendCSSTextForItems(result);
  result.Append('}');
  return result.ToString();
}

scoped_refptr<MediaQuerySet> CSSContainerRule::ContainerQueries() const {
  return To<StyleRuleContainer>(group_rule_.Get())
      ->GetContainerQuery()
      .MediaQueries();
}

MediaList* CSSContainerRule::container() const {
  if (!ContainerQueries())
    return nullptr;
  if (!media_cssom_wrapper_) {
    media_cssom_wrapper_ = MakeGarbageCollected<MediaList>(
        ContainerQueries(), const_cast<CSSContainerRule*>(this));
  }
  return media_cssom_wrapper_.Get();
}

const AtomicString& CSSContainerRule::Name() const {
  return To<StyleRuleContainer>(group_rule_.Get())->GetContainerQuery().Name();
}

void CSSContainerRule::Reattach(StyleRuleBase* rule) {
  CSSConditionRule::Reattach(rule);
  if (media_cssom_wrapper_ && ContainerQueries())
    media_cssom_wrapper_->Reattach(ContainerQueries());
}

void CSSContainerRule::Trace(Visitor* visitor) const {
  visitor->Trace(media_cssom_wrapper_);
  CSSConditionRule::Trace(visitor);
}
}  // namespace blink
