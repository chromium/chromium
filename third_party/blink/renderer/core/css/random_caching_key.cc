// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/random_caching_key.h"

#include <cstddef>

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

RandomCachingKey* RandomCachingKey::Create(
    RandomValueSharing random_value_sharing,
    const Element* element,
    AtomicString property_name,
    size_t property_value_index) {
  DCHECK(!random_value_sharing.IsFixed());
  const Element* element_for_caching =
      random_value_sharing.IsElementShared() ? nullptr : element;
  AtomicString ident = random_value_sharing.GetIdent();
  if (random_value_sharing.IsAuto()) {
    StringBuilder str;
    str.Append(property_name);
    str.AppendNumber(property_value_index);
    ident = str.ToAtomicString();
  }
  return MakeGarbageCollected<RandomCachingKey>(ident, element_for_caching);
}

bool RandomCachingKey::operator==(const RandomCachingKey& other) const {
  if (!element_ && !other.element_) {
    return ident_ == other.ident_;
  }
  if (!element_ || !other.element_) {
    return false;
  }
  return ident_ == other.ident_ && element_ == other.element_;
}

unsigned RandomCachingKey::GetHash() const {
  unsigned hash = blink::GetHash(ident_);
  if (element_) {
    AddIntToHash(hash, blink::GetHash(element_.Get()));
  }
  return hash;
}

void RandomCachingKey::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
}

}  // namespace blink
