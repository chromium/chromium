// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace {

struct Empty {};

struct StackAllocatedType {
  STACK_ALLOCATED();
};

static_assert(!WTF::IsStackAllocatedType<Empty>::value,
              "Failed to detect STACK_ALLOCATED macro.");
static_assert(WTF::IsStackAllocatedType<StackAllocatedType>::value,
              "Failed to detect STACK_ALLOCATED macro.");

}  // namespace
