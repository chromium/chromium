// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_CUSTOM_SPACES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_CUSTOM_SPACES_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/cppgc/custom-space.h"

namespace blink {

// The following defines custom spaces that are used to partition Oilpan's heap.
// Each custom space is assigned to a type partition using `cppgc::SpaceTrait`.
// It is expected that `kSpaceIndex` uniquely identifies a space and that the
// indices of all custom spaces form a sequence starting at 0. See
// `cppgc::CustomSpace` for details.

class PLATFORM_EXPORT HeapVectorBackingSpace
    : public cppgc::CustomSpace<HeapVectorBackingSpace> {
 public:
  static constexpr cppgc::CustomSpaceIndex kSpaceIndex = 0;
  static constexpr bool kSupportsCompaction = true;
};

class PLATFORM_EXPORT HeapHashTableBackingSpace
    : public cppgc::CustomSpace<HeapHashTableBackingSpace> {
 public:
  static constexpr cppgc::CustomSpaceIndex kSpaceIndex = 1;
  static constexpr bool kSupportsCompaction = true;
};

class PLATFORM_EXPORT NodeSpace : public cppgc::CustomSpace<NodeSpace> {
 public:
  static constexpr cppgc::CustomSpaceIndex kSpaceIndex = 2;
};

class PLATFORM_EXPORT CSSValueSpace : public cppgc::CustomSpace<CSSValueSpace> {
 public:
  static constexpr cppgc::CustomSpaceIndex kSpaceIndex = 3;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_CUSTOM_SPACES_H_
