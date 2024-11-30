// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_consumer.h"

namespace blink::v8_compile_hints {

void V8CrowdsourcedCompileHintsConsumer::SetData(
    base::span<const int64_t> memory) {
  if (memory.size() != kBloomFilterInt32Count / 2) {
    return;
  }
  data_ = base::MakeRefCounted<Data>();
  auto bloom_data = data_->bloom_.GetRawData();
  static_assert(sizeof(unsigned) == sizeof(int32_t));
  for (int i = 0; i < kBloomFilterInt32Count / 2; ++i) {
    bloom_data[2 * i] = static_cast<unsigned>(memory[i] & ((1LL << 32) - 1));
    bloom_data[2 * i + 1] = memory[i] >> 32;
  }
}

bool V8CrowdsourcedCompileHintsConsumer::CompileHintCallback(
    int position,
    void* raw_data_and_script_name_hash) {
  if (raw_data_and_script_name_hash == nullptr) {
    return false;
  }
  // The caller guarantees that this pointer is live.
  auto* data_and_script_name_hash =
      reinterpret_cast<DataAndScriptNameHash*>(raw_data_and_script_name_hash);
  auto hash = v8_compile_hints::CombineHash(
      data_and_script_name_hash->script_name_hash_, position);

  return data_and_script_name_hash->data_->bloom_.MayContain(hash);
}

std::unique_ptr<V8CrowdsourcedCompileHintsConsumer::DataAndScriptNameHash>
V8CrowdsourcedCompileHintsConsumer::GetDataWithScriptNameHash(
    uint32_t script_name_hash) {
  return std::make_unique<
      V8CrowdsourcedCompileHintsConsumer::DataAndScriptNameHash>(
      data_, script_name_hash);
}

}  // namespace blink::v8_compile_hints
