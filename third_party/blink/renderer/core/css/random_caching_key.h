// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RANDOM_CACHING_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RANDOM_CACHING_KEY_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class RandomValueSharing;

// RandomCachingKey serves as the key for random base value cache stored in the
// StyleEngine.
// https://drafts.csswg.org/css-values-5/#random-caching-key
class RandomCachingKey : public GarbageCollected<RandomCachingKey> {
 public:
  RandomCachingKey(base::PassKey<RandomCachingKey>,
                   AtomicString name,
                   const Element* element)
      : name_(name), element_(element) {}
  static RandomCachingKey* Create(
      const RandomValueSharing& random_value_sharing,
      const Element* element);
  bool operator==(const RandomCachingKey& other) const;
  unsigned GetHash() const;
  void Trace(Visitor* visitor) const;
  AtomicString Name() const { return name_; }

 private:
  AtomicString name_;
  Member<const Element> element_;
};

template <>
struct HashTraits<Member<RandomCachingKey>>
    : MemberHashTraits<RandomCachingKey> {
  static unsigned GetHash(const Member<RandomCachingKey>& key) {
    return key ? key->GetHash() : 0;
  }

  static bool Equal(const Member<RandomCachingKey>& a,
                    const Member<RandomCachingKey>& b) {
    if (!a) {
      return !b;
    }
    if (!b) {
      return false;
    }
    return *a == *b;
  }

  // True because a default-constructed Member (nullptr) is distinct from
  // any valid Member<RandomCachingKey> instance, and deleted slots are also
  // distinct.
  static constexpr bool kSafeToCompareToEmptyOrDeleted = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RANDOM_CACHING_KEY_H_
