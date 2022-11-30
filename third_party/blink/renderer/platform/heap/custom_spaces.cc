// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/custom_spaces.h"

namespace blink {

// static
constexpr cppgc::CustomSpaceIndex HeapVectorBackingSpace::kSpaceIndex;

// static
constexpr cppgc::CustomSpaceIndex HeapHashTableBackingSpace::kSpaceIndex;

// static
constexpr cppgc::CustomSpaceIndex NodeSpace::kSpaceIndex;

// static
constexpr cppgc::CustomSpaceIndex CSSValueSpace::kSpaceIndex;

// static
constexpr cppgc::CustomSpaceIndex LayoutObjectSpace::kSpaceIndex;

}  // namespace blink
