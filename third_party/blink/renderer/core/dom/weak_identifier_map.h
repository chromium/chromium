// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_WEAK_IDENTIFIER_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_WEAK_IDENTIFIER_MAP_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

// TODO(sof): WeakIdentifierMap<> belongs (out) in wtf/, but
// cannot until GarbageCollected<> can be used from WTF.

template <typename T, typename IdentifierType, bool isGarbageCollected>
class WeakIdentifierMapBase {
  USING_FAST_MALLOC(WeakIdentifierMapBase);

 protected:
  using ObjectToIdentifier = HashMap<T*, IdentifierType>;
  using IdentifierToObject = HashMap<IdentifierType, T*>;

  ObjectToIdentifier object_to_identifier_;
  IdentifierToObject identifier_to_object_;
};

template <typename T, typename IdentifierType>
class WeakIdentifierMapBase<T, IdentifierType, true>
    : public GarbageCollected<WeakIdentifierMapBase<T, IdentifierType, true>> {
 public:
  void Trace(blink::Visitor* visitor) {
    visitor->Trace(object_to_identifier_);
    visitor->Trace(identifier_to_object_);
  }

 protected:
  using ObjectToIdentifier = HeapHashMap<WeakMember<T>, IdentifierType>;
  using IdentifierToObject = HeapHashMap<IdentifierType, WeakMember<T>>;

  ObjectToIdentifier object_to_identifier_;
  IdentifierToObject identifier_to_object_;
};

template <typename T, typename IdentifierType = int>
class WeakIdentifierMap final
    : public WeakIdentifierMapBase<T,
                                   IdentifierType,
                                   IsGarbageCollectedType<T>::value> {
 public:
  static IdentifierType Identifier(T* object) {
    IdentifierType result = Instance().object_to_identifier_.at(object);

    if (WTF::IsHashTraitsEmptyValue<HashTraits<IdentifierType>>(result)) {
      result = Next();
      Instance().Put(object, result);
    }
    return result;
  }

  static T* Lookup(IdentifierType identifier) {
    return Instance().identifier_to_object_.at(identifier);
  }

  static void NotifyObjectDestroyed(T* object) {
    Instance().ObjectDestroyed(object);
  }

 private:
  static WeakIdentifierMap<T, IdentifierType>& Instance();

  WeakIdentifierMap() = default;

  static IdentifierType Next() {
    static IdentifierType last_id = 0;
    return ++last_id;
  }

  void Put(T* object, IdentifierType identifier) {
    DCHECK(object && !this->object_to_identifier_.Contains(object));
    this->object_to_identifier_.Set(object, identifier);
    this->identifier_to_object_.Set(identifier, object);
  }

  void ObjectDestroyed(T* object) {
    IdentifierType identifier = this->object_to_identifier_.Take(object);
    if (!WTF::IsHashTraitsEmptyValue<HashTraits<IdentifierType>>(identifier))
      this->identifier_to_object_.erase(identifier);
  }
};

#define DECLARE_WEAK_IDENTIFIER_MAP(T, ...)        \
  template <>                                      \
  WeakIdentifierMap<T, ##__VA_ARGS__>&             \
  WeakIdentifierMap<T, ##__VA_ARGS__>::Instance(); \
  extern template class WeakIdentifierMap<T, ##__VA_ARGS__>;

#define DEFINE_WEAK_IDENTIFIER_MAP(T, ...)                            \
  template class WeakIdentifierMap<T, ##__VA_ARGS__>;                 \
  template <>                                                         \
  WeakIdentifierMap<T, ##__VA_ARGS__>&                                \
  WeakIdentifierMap<T, ##__VA_ARGS__>::Instance() {                   \
    using RefType = WeakIdentifierMap<T, ##__VA_ARGS__>;              \
    DEFINE_STATIC_LOCAL(Persistent<RefType>, map_instance,            \
                        (new WeakIdentifierMap<T, ##__VA_ARGS__>())); \
    return *map_instance;                                             \
  }

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_WEAK_IDENTIFIER_MAP_H_
