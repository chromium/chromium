// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_UTILS_H_
#define SERVICES_WEBNN_WEBNN_UTILS_H_

#include <string>
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

std::string COMPONENT_EXPORT(WEBNN_SERVICE)
    OpTagToString(mojom::Operation::Tag tag);
std::string COMPONENT_EXPORT(WEBNN_SERVICE)
    OpKindToString(mojom::ArgMinMax::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_SERVICE)
    OpKindToString(mojom::ElementWiseBinary::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_SERVICE)
    OpKindToString(mojom::ElementWiseUnary::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_SERVICE)
    OpKindToString(mojom::Reduce::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_SERVICE)
    DataTypeToString(mojom::Operand::DataType type);

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_UTILS_H_
