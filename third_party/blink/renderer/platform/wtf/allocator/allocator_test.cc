// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

#include <variant>

namespace {

struct Empty {};

struct StackAllocatedType {
  STACK_ALLOCATED();
};

static_assert(!blink::IsStackAllocatedTypeV<Empty>,
              "Failed to detect STACK_ALLOCATED macro.");
static_assert(blink::IsStackAllocatedTypeV<StackAllocatedType>,
              "Failed to detect STACK_ALLOCATED macro.");

static_assert(blink::IsStackAllocatedTypeV<std::pair<int, StackAllocatedType>>,
              "Failed to detect STACK_ALLOCATED macro.");
static_assert(blink::IsStackAllocatedTypeV<std::optional<StackAllocatedType>>,
              "Failed to detect STACK_ALLOCATED macro.");
static_assert(
    blink::IsStackAllocatedTypeV<std::variant<int, StackAllocatedType>>,
    "Failed to detect STACK_ALLOCATED macro.");

}  // namespace
