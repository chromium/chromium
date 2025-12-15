// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/random_caching_key.h"

#include <cstddef>

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

RandomCachingKey* RandomCachingKey::Create(
    RandomValueSharing random_value_sharing,
    const Element* element) {
  DCHECK(!random_value_sharing.IsFixed());
  const Element* element_for_caching =
      random_value_sharing.IsElementShared() ? nullptr : element;
  AtomicString ident = random_value_sharing.GetIdent();
  DCHECK(!ident.IsNull());
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
