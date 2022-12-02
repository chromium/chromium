// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/anchor_scroll_value.h"

#include "third_party/blink/renderer/core/style/scoped_css_name.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// static
AnchorScrollValue* AnchorScrollValue::Implicit() {
  DEFINE_STATIC_LOCAL(Persistent<AnchorScrollValue>, instance,
                      {MakeGarbageCollected<AnchorScrollValue>(
                          base::PassKey<AnchorScrollValue>())});
  return instance;
}

AnchorScrollValue::AnchorScrollValue(base::PassKey<AnchorScrollValue>) {}

AnchorScrollValue::AnchorScrollValue(const ScopedCSSName& name) : name_(name) {}

bool AnchorScrollValue::operator==(const AnchorScrollValue& other) const {
  return base::ValuesEquivalent(name_, other.name_);
}

void AnchorScrollValue::Trace(Visitor* visitor) const {
  visitor->Trace(name_);
}

}  // namespace blink
