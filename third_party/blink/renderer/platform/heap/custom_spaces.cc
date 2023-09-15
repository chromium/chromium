// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/custom_spaces.h"

namespace blink {

// static
constexpr cppgc::CustomSpaceIndex
    CompactableHeapVectorBackingSpace::kSpaceIndex;

// static
constexpr cppgc::CustomSpaceIndex
    CompactableHeapHashTableBackingSpace::kSpaceIndex;

// static
constexpr cppgc::CustomSpaceIndex NodeSpace::kSpaceIndex;

// static
constexpr cppgc::CustomSpaceIndex CSSValueSpace::kSpaceIndex;

// static
constexpr cppgc::CustomSpaceIndex LayoutObjectSpace::kSpaceIndex;

// static
std::vector<std::unique_ptr<cppgc::CustomSpaceBase>>
CustomSpaces::CreateCustomSpaces() {
  std::vector<std::unique_ptr<cppgc::CustomSpaceBase>> spaces;
  spaces.emplace_back(std::make_unique<CompactableHeapVectorBackingSpace>());
  spaces.emplace_back(std::make_unique<CompactableHeapHashTableBackingSpace>());
  spaces.emplace_back(std::make_unique<NodeSpace>());
  spaces.emplace_back(std::make_unique<CSSValueSpace>());
  spaces.emplace_back(std::make_unique<LayoutObjectSpace>());
  return spaces;
}

}  // namespace blink
