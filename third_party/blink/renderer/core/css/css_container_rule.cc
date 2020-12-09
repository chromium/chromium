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
  // TODO(crbug.com/1145970): Spec and implement serialization.
  NOTIMPLEMENTED();
  return "";
}

}  // namespace blink
