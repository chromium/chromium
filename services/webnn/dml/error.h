// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_ERROR_H_
#define SERVICES_WEBNN_DML_ERROR_H_

#include <winerror.h>

#include "base/logging.h"

namespace webnn::dml {

#define RETURN_IF_FAILED(d3d_func)                                 \
  do {                                                             \
    HRESULT hr = d3d_func;                                         \
    if (FAILED(hr)) {                                              \
      LOG(ERROR) << "[WebNN] Failed to call " << #d3d_func << ": " \
                 << logging::SystemErrorCodeToString(hr);          \
      return hr;                                                   \
    }                                                              \
  } while (0)

#define RETURN_UNEXPECTED_IF_FAILED(d3d_func)                      \
  do {                                                             \
    HRESULT hr = d3d_func;                                         \
    if (FAILED(hr)) {                                              \
      LOG(ERROR) << "[WebNN] Failed to call " << #d3d_func << ": " \
                 << logging::SystemErrorCodeToString(hr);          \
      return base::unexpected(hr);                                 \
    }                                                              \
  } while (0)

#define RETURN_NULL_IF_FAILED(d3d_func)                            \
  do {                                                             \
    HRESULT hr = d3d_func;                                         \
    if (FAILED(hr)) {                                              \
      LOG(ERROR) << "[WebNN] Failed to call " << #d3d_func << ": " \
                 << logging::SystemErrorCodeToString(hr);          \
      return nullptr;                                              \
    }                                                              \
  } while (0)

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_ERROR_H_
