// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/buildflags.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"

#if BUILDFLAG(USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/trace_traits.h"
#else  // !USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/trace_traits.h"
#endif  // !USE_V8_OILPAN

namespace blink {

template <WTF::WeakHandlingFlag weakness,
          typename T,
          typename Traits,
          bool = WTF::IsTraceableInCollectionTrait<Traits>::value,
          WTF::WeakHandlingFlag = WTF::WeakHandlingTrait<T>::value>
struct TraceCollectionIfEnabled;

template <WTF::WeakHandlingFlag weakness, typename T, typename Traits>
struct TraceCollectionIfEnabled<weakness,
                                T,
                                Traits,
                                false,
                                WTF::kNoWeakHandling> {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool IsAlive(const blink::LivenessBroker& info, const T&) {
    return true;
  }

  static void Trace(Visitor*, const void*) {
    static_assert(!WTF::IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
  }
};

template <typename T, typename Traits>
struct TraceCollectionIfEnabled<WTF::kNoWeakHandling,
                                T,
                                Traits,
                                false,
                                WTF::kWeakHandling> {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static void Trace(Visitor* visitor, const void* t) {
    WTF::TraceInCollectionTrait<WTF::kNoWeakHandling, T, Traits>::Trace(
        visitor, *reinterpret_cast<const T*>(t));
  }
};

template <WTF::WeakHandlingFlag weakness,
          typename T,
          typename Traits,
          bool,
          WTF::WeakHandlingFlag>
struct TraceCollectionIfEnabled {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool IsAlive(const blink::LivenessBroker& info, const T& traceable) {
    return WTF::TraceInCollectionTrait<weakness, T, Traits>::IsAlive(info,
                                                                     traceable);
  }

  static void Trace(Visitor* visitor, const void* t) {
    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      weakness == WTF::kWeakHandling,
                  "Traits should be traced");
    WTF::TraceInCollectionTrait<weakness, T, Traits>::Trace(
        visitor, *reinterpret_cast<const T*>(t));
  }
};

}  // namespace blink

namespace WTF {

// Catch-all for types that have a way to trace that don't have special
// handling for weakness in collections.  This means that if this type
// contains WeakMember fields, they will simply be zeroed, but the entry
// will not be removed from the collection.  This always happens for
// things in vectors, which don't currently support special handling of
// weak elements.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling, T, Traits> {
  static bool IsAlive(const blink::LivenessBroker& info, const T& t) {
    return true;
  }

  static void Trace(blink::Visitor* visitor, const T& t) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value,
                  "T should be traceable");
    visitor->Trace(t);
  }
};

template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling, blink::WeakMember<T>, Traits> {
  static void Trace(blink::Visitor* visitor, const blink::WeakMember<T>& t) {
    // Extract raw pointer to avoid using the WeakMember<> overload in Visitor.
    visitor->TraceStrongly(t);
  }
};

// Catch-all for types that have HashTrait support for tracing with weakness.
// Empty to enforce specialization.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, T, Traits> {};

template <typename T, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, blink::WeakMember<T>, Traits> {
  static bool IsAlive(const blink::LivenessBroker& info,
                      const blink::WeakMember<T>& value) {
    return info.IsHeapObjectAlive(value);
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_
