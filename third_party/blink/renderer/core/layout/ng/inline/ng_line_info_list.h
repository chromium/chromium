// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_INFO_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_INFO_LIST_H_

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_info.h"

namespace blink {

// A `Vector` or `Deque`-like class for `NGLineInfo`, with a fixed maximum
// capacity `kCapacity`.
class NGLineInfoList {
  STACK_ALLOCATED();

 public:
  wtf_size_t Size() const { return size_; }
  bool IsEmpty() const { return !size_; }

  // Out-of-bounds `index` will hit `DCHECK` and returns the value at `index %
  // kCapacity`.
  NGLineInfo& operator[](wtf_size_t index) {
    DCHECK_LT(index, Size());
    return line_infos_[(start_index_ + index) % kCapacity];
  }
  // Out-of-bounds `index` will hit `DCHECK` and returns the value at `index %
  // kCapacity`.
  const NGLineInfo& operator[](wtf_size_t index) const {
    DCHECK_LT(index, Size());
    return line_infos_[(start_index_ + index) % kCapacity];
  }
  // If empty, this will hit `DCHECK`.
  NGLineInfo& Front() { return (*this)[0]; }
  // If empty, this will hit `DCHECK`.
  const NGLineInfo& Front() const { return (*this)[0]; }
  // If empty, this will hit `DCHECK`.
  NGLineInfo& Back() { return (*this)[Size() - 1]; }
  // If empty, this will hit `DCHECK`.
  const NGLineInfo& Back() const { return (*this)[Size() - 1]; }

  void Shrink(wtf_size_t size) {
    DCHECK_LT(size, size_);
    size_ = size;
  }
  void Clear() { size_ = start_index_ = 0; }

  NGLineInfo& Append() {
    DCHECK_LT(size_, kCapacity);
    ++size_;
    return Back();
  }

  // Get the cached `NGLineInfo` for the `break_token`, remove it from this
  // list, and set `is_cached_out` to `true`. If it doesn't exist, returns an
  // unused instance. The unused instance may be a new instance or a used
  // instance. Callsites are expected to call `NGLineInfo::Reset()`.
  NGLineInfo& Get(const NGInlineBreakToken* break_token, bool& is_cached_out);

  static constexpr wtf_size_t kCapacity = 4;

 private:
  NGLineInfo& UnusedInstance() {
    DCHECK(IsEmpty());
    return line_infos_[0];
  }
  void RemoveFront() {
    DCHECK_GT(size_, 0u);
    --size_;
    start_index_ = (start_index_ + 1) % kCapacity;
  }

  wtf_size_t size_ = 0;
  wtf_size_t start_index_ = 0;
  NGLineInfo line_infos_[kCapacity];
};

inline NGLineInfo& NGLineInfoList::Get(const NGInlineBreakToken* break_token,
                                       bool& is_cached_out) {
  DCHECK(!is_cached_out);
  if (IsEmpty()) {
    return UnusedInstance();
  }

  NGLineInfo& line_info = Front();
  if (break_token ? line_info.Start() == break_token->Start()
                  : line_info.Start().IsZero()) {
    RemoveFront();
    is_cached_out = true;
    return line_info;
  }

  Clear();
  return UnusedInstance();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_INFO_LIST_H_
