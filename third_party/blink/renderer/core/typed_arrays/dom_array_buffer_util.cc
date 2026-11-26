// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_util.h"

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

std::pair<ArrayBufferContents, base::span<uint8_t>> TransferArrayBufferView(
    v8::Isolate* isolate,
    DOMArrayBufferView* view,
    ExceptionState& exception_state) {
  CHECK(!view->IsShared());
  // Capture these before transferring, as the view is detached afterwards.
  const size_t byte_offset = view->byteOffset();
  const size_t byte_length = view->byteLength();
  ArrayBufferContents contents;
  if (!view->buffer()->Transfer(isolate, contents, exception_state)) {
    return {};
  }
  base::span<uint8_t> span =
      contents.ByteSpan().subspan(byte_offset, byte_length);
  return {std::move(contents), span};
}

void RelaxedAtomicReadMemcpy(base::span<uint8_t> dst,
                             base::span<const uint8_t> src) {
  CHECK_EQ(dst.size(), src.size());
  CHECK(dst.empty() ||
        reinterpret_cast<uintptr_t>(dst.data()) + dst.size() <=
            reinterpret_cast<uintptr_t>(src.data()) ||
        reinterpret_cast<uintptr_t>(src.data()) + src.size() <=
            reinterpret_cast<uintptr_t>(dst.data()));
  static_assert(std::atomic_ref<uint8_t>::required_alignment == 1);
  using Word = uintptr_t;
  static_assert(std::atomic_ref<Word>::is_always_lock_free);
  constexpr size_t kWordAlignment = std::atomic_ref<Word>::required_alignment;
  static_assert(std::has_single_bit(kWordAlignment));
  static_assert(kWordAlignment >= alignof(Word));
  static_assert(sizeof(Word) % kWordAlignment == 0);

  // C++20 std::atomic_ref<T> requires a non-const T (LWG 3508); `const_cast` is
  // safe here because only `.load(std::memory_order_relaxed)` is performed.
  while (!src.empty() && (reinterpret_cast<uintptr_t>(src.data()) &
                          (kWordAlignment - 1)) != 0) {
    uint8_t* src_byte = const_cast<uint8_t*>(src.take_first<1u>().data());
    dst.take_first<1u>()[0] =
        std::atomic_ref<uint8_t>(*src_byte).load(std::memory_order_relaxed);
  }

  while (src.size() >= sizeof(Word)) {
    Word* src_word = reinterpret_cast<Word*>(
        const_cast<uint8_t*>(src.take_first<sizeof(Word)>().data()));
    const Word word =
        std::atomic_ref<Word>(*src_word).load(std::memory_order_relaxed);
    dst.take_first<sizeof(Word)>().copy_from_nonoverlapping(
        base::byte_span_from_ref(word));
  }

  while (!src.empty()) {
    uint8_t* src_byte = const_cast<uint8_t*>(src.take_first<1u>().data());
    dst.take_first<1u>()[0] =
        std::atomic_ref<uint8_t>(*src_byte).load(std::memory_order_relaxed);
  }
}

}  // namespace blink
