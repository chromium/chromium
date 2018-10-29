// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_WEBUI_GRAPH_DUMP_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_WEBUI_GRAPH_DUMP_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/mojom/webui_graph_dump.mojom.h"

namespace resource_coordinator {

class CoordinationUnitGraph;

class WebUIGraphDumpImpl : public mojom::WebUIGraphDump {
 public:
  explicit WebUIGraphDumpImpl(CoordinationUnitGraph* graph);
  ~WebUIGraphDumpImpl() override;

  // WebUIGraphDump implementation.
  void GetCurrentGraph(GetCurrentGraphCallback callback) override;

  // Bind this instance to |request| with the |error_handler|.
  void Bind(mojom::WebUIGraphDumpRequest request,
            base::OnceClosure error_handler);

 private:
  CoordinationUnitGraph* graph_;
  mojo::Binding<mojom::WebUIGraphDump> binding_;

  DISALLOW_COPY_AND_ASSIGN(WebUIGraphDumpImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_WEBUI_GRAPH_DUMP_IMPL_H_
