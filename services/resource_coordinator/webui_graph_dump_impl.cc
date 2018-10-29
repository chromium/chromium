// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/webui_graph_dump_impl.h"

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"

namespace resource_coordinator {

WebUIGraphDumpImpl::WebUIGraphDumpImpl(CoordinationUnitGraph* graph)
    : graph_(graph), binding_(this) {
  DCHECK(graph);
}

WebUIGraphDumpImpl::~WebUIGraphDumpImpl() {}

void WebUIGraphDumpImpl::Bind(mojom::WebUIGraphDumpRequest request,
                              base::OnceClosure error_handler) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(std::move(error_handler));
}

void WebUIGraphDumpImpl::GetCurrentGraph(GetCurrentGraphCallback callback) {
  mojom::WebUIGraphPtr graph = mojom::WebUIGraph::New();

  {
    auto processes = graph_->GetAllProcessCoordinationUnits();
    graph->processes.reserve(processes.size());
    for (auto* process : processes) {
      mojom::WebUIProcessInfoPtr process_info = mojom::WebUIProcessInfo::New();

      process_info->id = process->id().id;
      process_info->pid = process->process_id();
      process_info->cumulative_cpu_usage = process->cumulative_cpu_usage();
      process_info->private_footprint_kb = process->private_footprint_kb();

      graph->processes.push_back(std::move(process_info));
    }
  }

  {
    auto frames = graph_->GetAllFrameCoordinationUnits();
    graph->frames.reserve(frames.size());
    for (auto* frame : frames) {
      mojom::WebUIFrameInfoPtr frame_info = mojom::WebUIFrameInfo::New();

      frame_info->id = frame->id().id;

      auto* parent_frame = frame->GetParentFrameCoordinationUnit();
      frame_info->parent_frame_id = parent_frame ? parent_frame->id().id : 0;

      auto* process = frame->GetProcessCoordinationUnit();
      frame_info->process_id = process ? process->id().id : 0;

      graph->frames.push_back(std::move(frame_info));
    }
  }

  {
    auto pages = graph_->GetAllPageCoordinationUnits();
    graph->pages.reserve(pages.size());
    for (auto* page : pages) {
      mojom::WebUIPageInfoPtr page_info = mojom::WebUIPageInfo::New();

      page_info->id = page->id().id;
      page_info->main_frame_url = page->main_frame_url();

      auto* main_frame = page->GetMainFrameCoordinationUnit();
      page_info->main_frame_id = main_frame ? main_frame->id().id : 0;

      graph->pages.push_back(std::move(page_info));
    }
  }
  std::move(callback).Run(std::move(graph));
}

}  // namespace resource_coordinator
