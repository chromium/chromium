// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_UTILS_COREML_H_
#define SERVICES_WEBNN_COREML_UTILS_COREML_H_

#include <CoreML/CoreML.h>

#include "base/component_export.h"
#include "base/containers/span.h"

namespace webnn::coreml {

// Reads from `multi_array` into `buffer`. The length of `buffer` must exactly
// match the number of bytes of data represented by `multi_array`.
//
// TODO(crbug.com/333392274): Refactor this method to be async.
void API_AVAILABLE(macos(12.3)) COMPONENT_EXPORT(WEBNN_SERVICE)
    ReadFromMLMultiArray(MLMultiArray* multi_array, base::span<uint8_t> buffer);

// Writes `bytes_to_write` into `multi_array`, overwriting any data
// that was previously there. The length of `bytes_to_write` must exactly
// match the number of bytes of data represented by `multi_array`.
//
// TODO(crbug.com/333392274): Refactor this method to be async.
void API_AVAILABLE(macos(12.3)) COMPONENT_EXPORT(WEBNN_SERVICE)
    WriteToMLMultiArray(MLMultiArray* multi_array,
                        base::span<const uint8_t> bytes_to_write);

}  // namespace webnn::coreml

#endif  // SERVICES_WEBNN_COREML_UTILS_COREML_H_
