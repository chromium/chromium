// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_UTILS_COREML_H_
#define SERVICES_WEBNN_COREML_UTILS_COREML_H_

#include <CoreML/CoreML.h>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace webnn::coreml {

// Reads from `multi_array` into a newly-allocated `BigBuffer` containing the
// read bytes.
mojo_base::BigBuffer COMPONENT_EXPORT(WEBNN_SERVICE)
    ReadFromMLMultiArray(MLMultiArray* multi_array);

// Writes `bytes_to_write` into `multi_array`, overwriting any data
// that was previously there. The length of `bytes_to_write` must exactly
// match the number of bytes of data represented by `multi_array`.
void COMPONENT_EXPORT(WEBNN_SERVICE)
    WriteToMLMultiArray(MLMultiArray* multi_array,
                        base::span<const uint8_t> bytes_to_write);

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_UTILS_COREML_H_
