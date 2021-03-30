// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_TRACE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_TRACE_TRAITS_H_

#include <tuple>

#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "v8/include/cppgc/trace-trait.h"

namespace blink {

template <typename T, bool = WTF::IsTraceable<T>::value>
struct TraceIfNeeded;

template <typename T>
struct TraceIfNeeded<T, false> {
  STATIC_ONLY(TraceIfNeeded);
  static void Trace(Visitor*, const T&) {}
};

template <typename T>
struct TraceIfNeeded<T, true> {
  STATIC_ONLY(TraceIfNeeded);
  static void Trace(Visitor* visitor, const T& t) { visitor->Trace(t); }
};

}  // namespace blink

namespace cppgc {

// This trace trait for std::pair will null weak members if their referent is
// collected. If you have a collection that contain weakness it does not remove
// entries from the collection that contain nulled weak members.
template <typename T, typename U>
struct TraceTrait<std::pair<T, U>> {
  STATIC_ONLY(TraceTrait);

 public:
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, Trace};
  }

  static void Trace(Visitor* visitor, const std::pair<T, U>* pair) {
    blink::TraceIfNeeded<U>::Trace(visitor, pair->second);
    blink::TraceIfNeeded<T>::Trace(visitor, pair->first);
  }
};

}  // namespace cppgc

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_TRACE_TRAITS_H_
