// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONSTRUCT_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONSTRUCT_TRAITS_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

// `ConstructTraits` is used to construct individual elements in WTF
// collections. All in-place constructions that may assign Oilpan objects must
// be dispatched through `ConstructAndNotifyElement()` or on of the related
// construction methods.
template <typename T, typename Traits, typename Allocator>
class ConstructTraits {
  STATIC_ONLY(ConstructTraits);

 public:
  // Construct a single element that would otherwise be constructed using
  // placement new. The call needs to be paired with one of the notify versions.
  template <typename... Args>
  static T* Construct(void* location, Args&&... args) {
    return ::new (base::NotNullTag::kNotNull, location)
        T(std::forward<Args>(args)...);
  }

  // After constructing elements using memcopy or memmove (or similar)
  // `NotifyNewElement()` needs to be called to propagate that information.
  static void NotifyNewElement(T* element) {
    // Avoid any further checks for element types that are not actually
    // traceable.
    if constexpr (!IsTraceableV<T>) {
      return;
    }
    Allocator::template NotifyNewObject<T, Traits>(element);
  }

  // Combines `Construct()` with `NotifyNewElement()`. This is the simplest way
  // to safely construct an element (but may not be the fastest).
  template <typename... Args>
  static T* ConstructAndNotifyElement(void* location, Args&&... args) {
    T* object = Construct(location, std::forward<Args>(args)...);
    NotifyNewElement(object);
    return object;
  }

  // Same as `NotifyNewElement()` for ranges of elements.
  static void NotifyNewElements(base::span<T> elements) {
    // Avoid any further checks for element types that are not actually
    // traceable.
    if constexpr (!IsTraceableV<T>) {
      return;
    }
    if (elements.empty()) {
      return;
    }
    Allocator::template NotifyNewObjects<T, Traits>(elements);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_CONSTRUCT_TRAITS_H_
