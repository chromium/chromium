// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"

#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// static
AnchorSpecifierValue* AnchorSpecifierValue::Default() {
  DEFINE_STATIC_LOCAL(
      Persistent<AnchorSpecifierValue>, instance,
      {MakeGarbageCollected<AnchorSpecifierValue>(
          base::PassKey<AnchorSpecifierValue>(), Type::kDefault)});
  return instance;
}

// static
AnchorSpecifierValue* AnchorSpecifierValue::Implicit() {
  DEFINE_STATIC_LOCAL(
      Persistent<AnchorSpecifierValue>, instance,
      {MakeGarbageCollected<AnchorSpecifierValue>(
          base::PassKey<AnchorSpecifierValue>(), Type::kImplicit)});
  return instance;
}

AnchorSpecifierValue::AnchorSpecifierValue(base::PassKey<AnchorSpecifierValue>,
                                           Type type)
    : type_(type) {
  DCHECK_NE(type, Type::kNamed);
}

AnchorSpecifierValue::AnchorSpecifierValue(const ScopedCSSName& name)
    : type_(Type::kNamed), name_(name) {}

bool AnchorSpecifierValue::operator==(const AnchorSpecifierValue& other) const {
  return type_ == other.type_ && base::ValuesEquivalent(name_, other.name_);
}

void AnchorSpecifierValue::Trace(Visitor* visitor) const {
  visitor->Trace(name_);
}

}  // namespace blink
