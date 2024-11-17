// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/directive.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_directive_type.h"

namespace blink {

Directive::Directive(Type type) : type_(type) {}
Directive::~Directive() = default;

Directive::Type Directive::GetType() const {
  return type_;
}

V8DirectiveType Directive::type() const {
  switch (type_) {
    case kUnknown:
      NOTREACHED();
    case kText:
      return V8DirectiveType(V8DirectiveType::Enum::kText);
    case kSelector:
      return V8DirectiveType(V8DirectiveType::Enum::kSelector);
  }

  NOTREACHED();
}

String Directive::toString() const {
  return ToStringImpl();
}

void Directive::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
