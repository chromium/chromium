// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_EPCONTEXT_MODEL_VALIDATOR_TEST_SUPPORT_H_
#define SERVICES_WEBNN_ORT_EPCONTEXT_MODEL_VALIDATOR_TEST_SUPPORT_H_

#include <cstdint>
#include <string_view>
#include <vector>

#include "base/containers/span.h"

// Byte builders that assemble the ONNX ModelProto envelope the EPContext model
// validator parses. Shared by the tests and the fuzzer seed so the encoding
// lives in one place.
namespace webnn::ort {

using Bytes = std::vector<uint8_t>;

// Builds a NodeProto with the given op_type and domain and no attributes.
Bytes BuildNode(std::string_view op_type, std::string_view domain);

// Builds an EPContext (com.microsoft) NodeProto, optionally carrying an
// `ep_cache_context` attribute with the given blob bytes.
Bytes BuildEpContextNode(base::span<const uint8_t> blob = {});

// Builds a GraphProto containing the given nodes verbatim.
Bytes BuildGraph(const std::vector<Bytes>& nodes);

// Builds an ONNX ModelProto with one `graph` field per entry of `graphs`.
Bytes BuildModelFromGraphs(const std::vector<Bytes>& graphs);

// Builds an ONNX ModelProto whose graph contains the given nodes verbatim.
Bytes BuildModel(const std::vector<Bytes>& nodes);

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_EPCONTEXT_MODEL_VALIDATOR_TEST_SUPPORT_H_
