// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_RANGE_H_
#define SERVICES_WEBNN_PUBLIC_CPP_RANGE_H_

namespace webnn {

// The range attribute of slice operator.
struct Range {
  // The starting point of the window to slice.
  uint32_t start;
  // The window size.
  uint32_t size;
  // Indicates how many elements to traverse when slicing.
  uint32_t stride;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_RANGE_H_
