// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/directive.h"

#include "base/notreached.h"

namespace blink {

Directive::Directive(Type type) : type_(type) {}
Directive::~Directive() = default;

Directive::Type Directive::GetType() const {
  return type_;
}

String Directive::type() const {
  DEFINE_STATIC_LOCAL(const String, text, ("text"));
  DEFINE_STATIC_LOCAL(const String, selector, ("selector"));

  switch (type_) {
    case kUnknown:
      NOTREACHED_IN_MIGRATION();
      return String();
    case kText:
      return text;
    case kSelector:
      return selector;
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

String Directive::toString() const {
  return ToStringImpl();
}

void Directive::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
