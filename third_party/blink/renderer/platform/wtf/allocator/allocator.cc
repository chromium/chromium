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

namespace WTF {

void AtomicReadMemcpy(void* to, const void* from, size_t bytes) {
  // Check alignment of |to| and |from|
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1));
  size_t* sizet_to = reinterpret_cast<size_t*>(to);
  const size_t* sizet_from = reinterpret_cast<const size_t*>(from);
  for (; bytes >= sizeof(size_t);
       bytes -= sizeof(size_t), ++sizet_to, ++sizet_from) {
    *sizet_to = AsAtomicPtr(sizet_from)->load(std::memory_order_relaxed);
  }
  uint8_t* uint8t_to = reinterpret_cast<uint8_t*>(sizet_to);
  const uint8_t* uint8t_from = reinterpret_cast<const uint8_t*>(sizet_from);
  for (; bytes > 0; bytes -= sizeof(uint8_t), ++uint8t_to, ++uint8t_from) {
    *uint8t_to = AsAtomicPtr(uint8t_from)->load(std::memory_order_relaxed);
  }
  DCHECK_EQ(0u, bytes);
}

void AtomicWriteMemcpy(void* to, const void* from, size_t bytes) {
  // Check alignment of |to| and |from|
  DCHECK_EQ(0u, reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1));
  DCHECK_EQ(0u, reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1));
  size_t* sizet_to = reinterpret_cast<size_t*>(to);
  const size_t* sizet_from = reinterpret_cast<const size_t*>(from);
  for (; bytes >= sizeof(size_t);
       bytes -= sizeof(size_t), ++sizet_to, ++sizet_from) {
    AsAtomicPtr(sizet_to)->store(*sizet_from, std::memory_order_relaxed);
  }
  uint8_t* uint8t_to = reinterpret_cast<uint8_t*>(sizet_to);
  const uint8_t* uint8t_from = reinterpret_cast<const uint8_t*>(sizet_from);
  for (; bytes > 0; bytes -= sizeof(uint8_t), ++uint8t_to, ++uint8t_from) {
    AsAtomicPtr(uint8t_to)->store(*uint8t_from, std::memory_order_relaxed);
  }
  DCHECK_EQ(0u, bytes);
}

void AtomicMemzero(void* buf, size_t bytes) {
  // Check alignment of |buf|
  DCHECK_EQ(0u, reinterpret_cast<size_t>(buf) & (sizeof(size_t) - 1));
  size_t* sizet_buf = reinterpret_cast<size_t*>(buf);
  for (; bytes >= sizeof(size_t); bytes -= sizeof(size_t), ++sizet_buf) {
    AsAtomicPtr(sizet_buf)->store(0, std::memory_order_relaxed);
  }
  uint8_t* uint8t_buf = reinterpret_cast<uint8_t*>(sizet_buf);
  for (; bytes > 0; bytes -= sizeof(uint8_t), ++uint8t_buf) {
    AsAtomicPtr(uint8t_buf)->store(0, std::memory_order_relaxed);
  }
  DCHECK_EQ(0u, bytes);
}

}  // namespace WTF
