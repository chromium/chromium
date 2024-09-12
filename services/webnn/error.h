// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ERROR_H_
#define SERVICES_WEBNN_ERROR_H_

#include <string>

#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"

namespace webnn {

// Bad messages Mojo errors.
inline constexpr char kBadMessageInvalidGraph[] =
    "Invalid graph from renderer.";
inline constexpr char kBadMessageInvalidTensor[] =
    "Invalid tensor from renderer.";
inline constexpr char kBadMessageOnBuiltGraphBuilder[] =
    "Invalid message on an MLGraphBuilder which has already built a graph.";

template <typename MojoResultType>
mojo::StructPtr<MojoResultType> ToError(const mojom::Error::Code& error_code,
                                        const std::string& error_message) {
  return MojoResultType::NewError(mojom::Error::New(error_code, error_message));
}

}  // namespace webnn

#endif  // SERVICES_WEBNN_ERROR_H_
