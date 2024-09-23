// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"

namespace blink {

#define DEFINE_WEBNN_ERROR_CODE_MAPPING(error_code)    \
  case webnn::mojom::blink::Error::Code::error_code: { \
    return DOMExceptionCode::error_code;               \
  }

DOMExceptionCode WebNNErrorCodeToDOMExceptionCode(
    webnn::mojom::blink::Error::Code error_code) {
  switch (error_code) {
    DEFINE_WEBNN_ERROR_CODE_MAPPING(kUnknownError)
    DEFINE_WEBNN_ERROR_CODE_MAPPING(kNotSupportedError)
  }
}

}  // namespace blink
