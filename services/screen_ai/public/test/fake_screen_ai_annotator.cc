// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/public/test/fake_screen_ai_annotator.h"

#include <utility>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/rect_f.h"

namespace screen_ai::test {

FakeScreenAIAnnotator::FakeScreenAIAnnotator(bool create_empty_result)
    : create_empty_result_(create_empty_result) {}

FakeScreenAIAnnotator::~FakeScreenAIAnnotator() = default;

void FakeScreenAIAnnotator::PerformOcrAndReturnAXTreeUpdate(
    const ::SkBitmap& image,
    PerformOcrAndReturnAXTreeUpdateCallback callback) {
  ui::AXTreeUpdate update;
  if (!create_empty_result_) {
    update.has_tree_data = true;
    // TODO(nektar): Add a tree ID as well and update tests.
    // update.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    update.tree_data.title = "Screen AI";
    update.root_id = next_node_id_;
    ui::AXNodeData node;
    node.id = next_node_id_;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked("Testing");
    node.relative_bounds.bounds = gfx::RectF(1.0f, 2.0f, 1.0f, 2.0f);
    update.nodes = {node};
    --next_node_id_;
  }
  std::move(callback).Run(update);
}

void FakeScreenAIAnnotator::PerformOcrAndReturnAnnotation(
    const ::SkBitmap& image,
    PerformOcrAndReturnAnnotationCallback callback) {
  auto annotation = screen_ai::mojom::VisualAnnotation::New();
  std::move(callback).Run(std::move(annotation));
}

mojo::PendingRemote<mojom::ScreenAIAnnotator>
FakeScreenAIAnnotator::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeScreenAIAnnotator::SetClientType(mojom::OcrClientType) {}

}  // namespace screen_ai::test
