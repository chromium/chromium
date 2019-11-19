// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONSTRUCT_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONSTRUCT_TRAITS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace WTF {

// ConstructTraits is used to construct elements in WTF collections. All
// in-place constructions that may assign Oilpan objects must be dispatched
// through ConstructAndNotifyElement.
template <typename T, typename Traits, typename Allocator>
class ConstructTraits {
  STATIC_ONLY(ConstructTraits);

 public:
  // Construct a single element that would otherwise be constructed using
  // placement new.
  template <typename... Args>
  static T* Construct(void* location, Args&&... args) {
    return new (NotNull, location) T(std::forward<Args>(args)...);
  }

  // After constructing elements using memcopy or memmove (or similar)
  // |NotifyNewElement| needs to be called to propagate that information.
  static void NotifyNewElement(T* element) {
    Allocator::template NotifyNewObject<T, Traits>(element);
  }

  template <typename... Args>
  static T* ConstructAndNotifyElement(void* location, Args&&... args) {
    T* object = Construct(location, std::forward<Args>(args)...);
    NotifyNewElement(object);
    return object;
  }

  static void NotifyNewElements(T* array, size_t len) {
    Allocator::template NotifyNewObjects<T, Traits>(array, len);
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONSTRUCT_TRAITS_H_
