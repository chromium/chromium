// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_error.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn {

class WebNNConstantOperand;
class WebNNContextImpl;

// Services-side connection to an `MLGraphBuilder`. Responsible for managing
// data associated with the graph builder.
//
// A `WebNNGraphBuilderImpl` may create at most one `WebNNGraphImpl`, when
// `CreateGraph()` is called. Once built, this graph does not depend on its
// builder and the builder will be destroyed.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNGraphBuilderImpl
    : public mojom::WebNNGraphBuilder {
 public:
  explicit WebNNGraphBuilderImpl(WebNNContextImpl& context);

  WebNNGraphBuilderImpl(const WebNNGraphBuilderImpl&) = delete;
  WebNNGraphBuilderImpl& operator=(const WebNNGraphBuilderImpl&) = delete;

  ~WebNNGraphBuilderImpl() override;

  // mojom::WebNNGraphBuilder
  void CreateGraph(mojom::GraphInfoPtr graph_info,
                   CreateGraphCallback callback) override;

  void SetId(mojo::ReceiverId id, base::PassKey<WebNNContextImpl> pass_key);

  // Return `ComputeResourceInfo` which describe graph constraints if it is
  // valid; otherwise null.
  [[nodiscard]] static std::optional<WebNNGraphImpl::ComputeResourceInfo>
  ValidateGraph(const ContextProperties& context_properties,
                const mojom::GraphInfo& graph_info);

  // Same as above, but just return true/false.
  [[nodiscard]] static bool IsValidForTesting(
      const ContextProperties& context_properties,
      const mojom::GraphInfo& graph_info);

  [[nodiscard]] static base::flat_map<uint64_t,
                                      std::unique_ptr<WebNNConstantOperand>>
  TakeConstants(mojom::GraphInfo& graph_info);

 private:
  void DidCreateGraph(
      CreateGraphCallback callback,
      base::expected<std::unique_ptr<WebNNGraphImpl>, mojom::ErrorPtr> result);

  void DestroySelf();

  SEQUENCE_CHECKER(sequence_checker_);

  // The `WebNNContextImpl` which owns and will outlive this object.
  const raw_ref<WebNNContextImpl> context_;

  // Set by the owning `context_` so this builder can identify itself when
  // requesting to be destroyed.
  mojo::ReceiverId id_;

  // Tracks whether `CreateGraph()` has been called. If so, any subsequent
  // incoming messages to the mojo pipe are signs of a misbehaving renderer.
  bool has_built_ = false;

  base::WeakPtrFactory<WebNNGraphBuilderImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_GRAPH_BUILDER_IMPL_H_
