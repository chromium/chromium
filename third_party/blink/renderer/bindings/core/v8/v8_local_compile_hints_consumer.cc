// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_consumer.h"

#include "base/containers/span_reader.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"

namespace blink::v8_compile_hints {

V8LocalCompileHintsConsumer::V8LocalCompileHintsConsumer(
    CachedMetadata* cached_metadata) {
  CHECK(cached_metadata);
  base::SpanReader reader(cached_metadata->Data());

  constexpr auto kLocalCompileHintsPrefixSize = sizeof(int64_t);
  if (!reader.Skip(kLocalCompileHintsPrefixSize) ||
      reader.remaining() % sizeof(int32_t) != 0) {
    rejected_ = true;
    return;
  }

  const size_t compile_hint_count = reader.remaining() / sizeof(int32_t);
  compile_hints_.reserve(static_cast<wtf_size_t>(compile_hint_count));

  // Read every int in a little-endian manner.
  int32_t hint = 0;
  while (reader.ReadI32LittleEndian(hint)) {
    compile_hints_.push_back(hint);
  }
  CHECK_EQ(compile_hint_count, compile_hints_.size());
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
