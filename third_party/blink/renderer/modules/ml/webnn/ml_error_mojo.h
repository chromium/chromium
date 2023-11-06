// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_ERROR_MOJO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_ERROR_MOJO_H_

#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"

namespace blink {

namespace blink_mojom = webnn::mojom::blink;

DOMExceptionCode ConvertWebNNErrorCodeToDOMExceptionCode(
    blink_mojom::Error::Code error_code);

template <typename MojoResultType>
mojo::StructPtr<MojoResultType> ToError(
    const blink_mojom::Error::Code& error_code,
    const WTF::String& error_message) {
  return MojoResultType::NewError(
      blink_mojom::Error::New(error_code, error_message));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_ERROR_MOJO_H_
