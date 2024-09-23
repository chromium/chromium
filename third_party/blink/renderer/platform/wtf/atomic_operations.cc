// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/atomic_operations.h"

namespace WTF {

namespace {

template <typename AlignmentType>
void AtomicReadMemcpyImpl(void* to, const void* from, size_t bytes) {
  // Check alignment of |to| and |from|.
  DCHECK_EQ(0u, static_cast<AlignmentType>(reinterpret_cast<size_t>(to)) &
                    (sizeof(AlignmentType) - 1));
  DCHECK_EQ(0u, static_cast<AlignmentType>(reinterpret_cast<size_t>(from)) &
                    (sizeof(AlignmentType) - 1));
  auto* sizet_to = reinterpret_cast<AlignmentType*>(to);
  const auto* sizet_from = reinterpret_cast<const AlignmentType*>(from);
  for (; bytes >= sizeof(AlignmentType);
       bytes -= sizeof(AlignmentType), ++sizet_to, ++sizet_from) {
    *sizet_to = AsAtomicPtr(sizet_from)->load(std::memory_order_relaxed);
  }

  uint32_t* uint32t_to = reinterpret_cast<uint32_t*>(sizet_to);
  const uint32_t* uint32t_from = reinterpret_cast<const uint32_t*>(sizet_from);
  if (sizeof(AlignmentType) == 8 && bytes >= 4) {
    *uint32t_to = AsAtomicPtr(uint32t_from)->load(std::memory_order_relaxed);
    bytes -= sizeof(uint32_t);
    ++uint32t_to;
    ++uint32t_from;
  }

  uint8_t* uint8t_to = reinterpret_cast<uint8_t*>(uint32t_to);
  const uint8_t* uint8t_from = reinterpret_cast<const uint8_t*>(uint32t_from);
  for (; bytes > 0; bytes -= sizeof(uint8_t), ++uint8t_to, ++uint8t_from) {
    *uint8t_to = AsAtomicPtr(uint8t_from)->load(std::memory_order_relaxed);
  }
  DCHECK_EQ(0u, bytes);
}

template <typename AlignmentType>
void AtomicWriteMemcpyImpl(void* to, const void* from, size_t bytes) {
  // Check alignment of |to| and |from|.
  DCHECK_EQ(0u, static_cast<AlignmentType>(reinterpret_cast<size_t>(to)) &
                    (sizeof(AlignmentType) - 1));
  DCHECK_EQ(0u, static_cast<AlignmentType>(reinterpret_cast<size_t>(from)) &
                    (sizeof(AlignmentType) - 1));
  auto* sizet_to = reinterpret_cast<AlignmentType*>(to);
  const auto* sizet_from = reinterpret_cast<const AlignmentType*>(from);
  for (; bytes >= sizeof(AlignmentType);
       bytes -= sizeof(AlignmentType), ++sizet_to, ++sizet_from) {
    AsAtomicPtr(sizet_to)->store(*sizet_from, std::memory_order_relaxed);
  }

  uint32_t* uint32t_to = reinterpret_cast<uint32_t*>(sizet_to);
  const uint32_t* uint32t_from = reinterpret_cast<const uint32_t*>(sizet_from);
  if (sizeof(AlignmentType) == 8 && bytes >= 4) {
    AsAtomicPtr(uint32t_to)->store(*uint32t_from, std::memory_order_relaxed);
    bytes -= sizeof(uint32_t);
    ++uint32t_to;
    ++uint32t_from;
  }

  uint8_t* uint8t_to = reinterpret_cast<uint8_t*>(uint32t_to);
  const uint8_t* uint8t_from = reinterpret_cast<const uint8_t*>(uint32t_from);
  for (; bytes > 0; bytes -= sizeof(uint8_t), ++uint8t_to, ++uint8t_from) {
    AsAtomicPtr(uint8t_to)->store(*uint8t_from, std::memory_order_relaxed);
  }
  DCHECK_EQ(0u, bytes);
}

template <typename AlignmentType>
void AtomicMemzeroImpl(void* buf, size_t bytes) {
  // Check alignment of |buf|.
  DCHECK_EQ(0u, static_cast<AlignmentType>(reinterpret_cast<size_t>(buf)) &
                    (sizeof(AlignmentType) - 1));
  auto* sizet_buf = reinterpret_cast<AlignmentType*>(buf);
  for (; bytes >= sizeof(AlignmentType);
       bytes -= sizeof(AlignmentType), ++sizet_buf) {
    AsAtomicPtr(sizet_buf)->store(0, std::memory_order_relaxed);
  }

  uint32_t* uint32t_buf = reinterpret_cast<uint32_t*>(sizet_buf);
  if (sizeof(AlignmentType) == 8 && bytes >= 4) {
    AsAtomicPtr(uint32t_buf)->store(0, std::memory_order_relaxed);
    bytes -= sizeof(uint32_t);
    ++uint32t_buf;
  }

  uint8_t* uint8t_buf = reinterpret_cast<uint8_t*>(uint32t_buf);
  for (; bytes > 0; bytes -= sizeof(uint8_t), ++uint8t_buf) {
    AsAtomicPtr(uint8t_buf)->store(0, std::memory_order_relaxed);
  }
  DCHECK_EQ(0u, bytes);
}

}  // namespace

void AtomicReadMemcpy(void* to, const void* from, size_t bytes) {
#if defined(ARCH_CPU_64_BITS)
  const size_t mod_to = reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1);
  const size_t mod_from = reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1);
  if (mod_to != 0 || mod_from != 0) {
    AtomicReadMemcpyImpl<uint32_t>(to, from, bytes);
    return;
  }
#endif  // defined(ARCH_CPU_64_BITS)
  AtomicReadMemcpyImpl<uintptr_t>(to, from, bytes);
}

void AtomicWriteMemcpy(void* to, const void* from, size_t bytes) {
#if defined(ARCH_CPU_64_BITS)
  const size_t mod_to = reinterpret_cast<size_t>(to) & (sizeof(size_t) - 1);
  const size_t mod_from = reinterpret_cast<size_t>(from) & (sizeof(size_t) - 1);
  if (mod_to != 0 || mod_from != 0) {
    AtomicWriteMemcpyImpl<uint32_t>(to, from, bytes);
    return;
  }
#endif  // defined(ARCH_CPU_64_BITS)
  AtomicWriteMemcpyImpl<uintptr_t>(to, from, bytes);
}

void AtomicMemzero(void* buf, size_t bytes) {
#if defined(ARCH_CPU_64_BITS)
  const size_t mod = reinterpret_cast<size_t>(buf) & (sizeof(size_t) - 1);
  if (mod != 0) {
    AtomicMemzeroImpl<uint32_t>(buf, bytes);
    return;
  }
#endif  // defined(ARCH_CPU_64_BITS)
  AtomicMemzeroImpl<uintptr_t>(buf, bytes);
}

}  // namespace WTF
