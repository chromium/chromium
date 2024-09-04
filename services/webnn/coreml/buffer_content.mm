// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/buffer_content.h"

#import <CoreML/CoreML.h>

#include "services/webnn/coreml/utils_coreml.h"

namespace webnn::coreml {

BufferContent::BufferContent(MLMultiArray* multi_array)
    : multi_array_(multi_array) {}

BufferContent::~BufferContent() = default;

void BufferContent::Read(base::span<uint8_t> buffer) const {
  ReadFromMLMultiArray(multi_array_, buffer);
}

void BufferContent::Write(base::span<const uint8_t> bytes_to_write) {
  WriteToMLMultiArray(multi_array_, bytes_to_write);
}

MLFeatureValue* BufferContent::AsFeatureValue() const {
  return [MLFeatureValue featureValueWithMultiArray:multi_array_];
}

}  // namespace webnn::coreml
