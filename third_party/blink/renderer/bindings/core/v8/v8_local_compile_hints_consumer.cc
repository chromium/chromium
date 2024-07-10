// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_consumer.h"

#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"

namespace blink::v8_compile_hints {

V8LocalCompileHintsConsumer::V8LocalCompileHintsConsumer(
    CachedMetadata* cached_metadata) {
  CHECK(cached_metadata);
  size_t length = cached_metadata->size();
  const uint8_t* data = cached_metadata->Data();

  constexpr auto kLocalCompileHintsPrefixSize = sizeof(int64_t);
  if (length < kLocalCompileHintsPrefixSize ||
      (length - kLocalCompileHintsPrefixSize) % sizeof(int) != 0) {
    rejected_ = true;
    return;
  }

  size_t ix = kLocalCompileHintsPrefixSize;
  size_t compile_hint_count =
      (length - kLocalCompileHintsPrefixSize) / sizeof(int);
  compile_hints_.reserve(static_cast<wtf_size_t>(compile_hint_count));
  // Read every int in a little-endian manner.
  for (size_t i = 0; i < compile_hint_count; ++i) {
    int hint = 0;
    for (size_t j = 0; j < sizeof(int); ++j) {
      hint |= (data[ix++] << (j * 8));
    }
    compile_hints_.push_back(hint);
  }
}

bool V8LocalCompileHintsConsumer::GetCompileHint(int pos, void* data) {
  auto* v8_local_compile_hints_consumer =
      reinterpret_cast<V8LocalCompileHintsConsumer*>(data);
  return v8_local_compile_hints_consumer->GetCompileHint(pos);
}

bool V8LocalCompileHintsConsumer::GetCompileHint(int pos) {
  while (current_index_ < compile_hints_.size() &&
         compile_hints_[current_index_] < pos) {
    ++current_index_;
  }
  if (current_index_ >= compile_hints_.size() ||
      compile_hints_[current_index_] > pos) {
    return false;
  }
  CHECK_EQ(compile_hints_[current_index_], pos);
  ++current_index_;
  return true;
}

}  // namespace blink::v8_compile_hints
