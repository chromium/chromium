// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ORT_STATUS_H_
#define SERVICES_WEBNN_ORT_ORT_STATUS_H_

#include <string>

#include "base/logging.h"

struct OrtStatus;

namespace webnn::ort {

namespace internal {

std::string OrtStatusFatalMessage(OrtStatus* status);

}  // namespace internal

#define CHECK_STATUS(expr)                                 \
  if (OrtStatus* status = (expr)) {                        \
    LOG(FATAL) << internal::OrtStatusFatalMessage(status); \
  }

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ORT_STATUS_H_
