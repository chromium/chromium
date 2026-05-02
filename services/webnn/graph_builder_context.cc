// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/graph_builder_context.h"

#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

GraphBuilderContext::GraphCreationResult::GraphCreationResult() = default;
GraphBuilderContext::GraphCreationResult::GraphCreationResult(
    GraphCreationResult&&) = default;
GraphBuilderContext::GraphCreationResult&
GraphBuilderContext::GraphCreationResult::operator=(GraphCreationResult&&) =
    default;
GraphBuilderContext::GraphCreationResult::~GraphCreationResult() = default;

}  // namespace webnn
