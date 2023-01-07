// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/tools/compositor_model_bench/render_models.h"

#include <string>
#include <utility>

#include "gpu/tools/compositor_model_bench/forward_render_model.h"

const char* ModelToString(RenderModel m) {
  switch (m) {
    case ForwardRenderModel:
      return "Forward Rendering";
    default:
      return "(unknown render model name)";
  }
}

RenderModelSimulator::RenderModelSimulator(std::unique_ptr<RenderNode> root)
    : root_(std::move(root)) {}

RenderModelSimulator::~RenderModelSimulator() {
}

std::unique_ptr<RenderModelSimulator> ConstructSimulationModel(
    RenderModel model,
    std::unique_ptr<RenderNode> render_tree_root,
    int window_width,
    int window_height) {
  switch (model) {
    case ForwardRenderModel:
      return std::make_unique<ForwardRenderSimulator>(
          std::move(render_tree_root), window_width, window_height);
    default:
      LOG(ERROR) << "Unrecognized render model. "
        "If we know its name, then it's..." << ModelToString(model);
      return nullptr;
  }
}
