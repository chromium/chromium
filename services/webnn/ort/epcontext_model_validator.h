// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_EPCONTEXT_MODEL_VALIDATOR_H_
#define SERVICES_WEBNN_ORT_EPCONTEXT_MODEL_VALIDATOR_H_

#include <cstdint>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/message.h"

namespace webnn::ort {

// The result of validating a compiled EPContext model.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WebNNOrtEpContextModelResult)
enum class EpContextModelResult {
  kAccepted = 0,
  kMalformed = 1,
  kNoGraph = 2,
  kDuplicateGraph = 3,
  kNonEpContextNode = 4,
  kWrongNodeCount = 5,
  kBadEmbedMode = 6,
  kMaxValue = kBadEmbedMode,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webnn/enums.xml:WebNNOrtEpContextModelResult)

// Defense-in-depth check at the Compiler->GPU trust boundary. `model_data` is
// the compiled EPContext ONNX model produced by the sandboxed (and possibly
// compromised) Compiler process for an EP that follows the ORT EPContext
// contract. Such a model is just a wrapper around the EP's precompiled blob:
// its graph carries at most one node, which must be an EPContext
// (com.microsoft) node. It has no nodes when constant folding in the Compiler
// reduces the whole graph to initializers; the outputs are then returned
// straight from those initializers and no kernel runs. This parses the ONNX
// envelope and confirms that shape, rejecting any model with several nodes, a
// stray non-EPContext node, or malformed protobuf. The node must also set
// `embed_mode` to 1, so that the EP never treats `ep_cache_context` as a file
// path.
//
// The node's blob is intentionally not inspected here: the execution provider
// and its backend validate its contents. This structural check just ensures
// the Compiler cannot smuggle extra ONNX nodes past the boundary for ORT to
// run, or make the EP open a file through `ep_cache_context`.
COMPONENT_EXPORT(WEBNN_SERVICE)
EpContextModelResult ValidateEpContextModel(
    base::span<const uint8_t> model_data);

// Prepares the compiled model `model_data` received from the Compiler for
// loading by ORT with `ep_name`. The bytes are first moved into private memory
// so the Compiler cannot change them after validation, then validated with
// ValidateEpContextModel() if `ep_name` compiles into an EPContext model. On
// rejection, runs `report_bad_message` and returns false.
COMPONENT_EXPORT(WEBNN_SERVICE)
bool PrepareCompiledModel(mojo_base::BigBuffer& model_data,
                          std::string_view ep_name,
                          mojo::ReportBadMessageCallback report_bad_message);

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_EPCONTEXT_MODEL_VALIDATOR_H_
