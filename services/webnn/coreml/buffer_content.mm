// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/buffer_content.h"

#import <CoreML/CoreML.h>

#include "base/functional/callback.h"
#include "services/webnn/coreml/utils_coreml.h"

namespace webnn::coreml {

BufferContent::BufferContent(MLMultiArray* multi_array)
    : multi_array_(multi_array) {}

BufferContent::~BufferContent() = default;

void BufferContent::Read(
    base::OnceCallback<void(mojo_base::BigBuffer)> result_callback) const {
  ReadFromMLMultiArray(multi_array_, std::move(result_callback));
}

void BufferContent::Write(base::span<const uint8_t> bytes_to_write,
                          base::OnceClosure done_closure) {
  WriteToMLMultiArray(multi_array_, bytes_to_write, std::move(done_closure));
}

MLFeatureValue* BufferContent::AsFeatureValue() const {
  return [MLFeatureValue featureValueWithMultiArray:multi_array_];
}

}  // namespace webnn::coreml
