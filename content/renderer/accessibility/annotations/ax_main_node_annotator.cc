// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_main_node_annotator.h"

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace content {

using blink::WebAXObject;
using blink::WebDocument;

AXMainNodeAnnotator::AXMainNodeAnnotator(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(render_accessibility) {
  DCHECK(render_accessibility_);
}

AXMainNodeAnnotator::~AXMainNodeAnnotator() = default;

void AXMainNodeAnnotator::EnableAnnotations() {
  if (annotator_remote_.is_bound() || !render_accessibility_->render_frame()) {
    return;
  }
  mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor> annotator;
  render_accessibility_->render_frame()
      ->GetBrowserInterfaceBroker()
      ->GetInterface(annotator.InitWithNewPipeAndPassReceiver());
  annotator_remote_.Bind(std::move(annotator));
}

void AXMainNodeAnnotator::CancelAnnotations() {
  if (!annotator_remote_.is_bound() ||
      render_accessibility_->GetAccessibilityMode().has_mode(
          GetAXModeToEnableAnnotations())) {
    return;
  }
  annotator_remote_.reset();
}

uint32_t AXMainNodeAnnotator::GetAXModeToEnableAnnotations() {
  return ui::AXMode::kAnnotateMainNode;
}

bool AXMainNodeAnnotator::HasAXActionToEnableAnnotations() {
  return false;
}

ax::mojom::Action AXMainNodeAnnotator::GetAXActionToEnableAnnotations() {
  NOTREACHED_NORETURN();
}

void AXMainNodeAnnotator::Annotate(const WebDocument& document,
                                   ui::AXTreeUpdate* update,
                                   bool load_complete) {
  if (main_node_id_ != ui::kInvalidAXNodeID) {
    // TODO: Replace with binary search as nodes should be in order by id.
    for (ui::AXNodeData& node : update->nodes) {
      if (node.id != main_node_id_) {
        continue;
      }
      // TODO: Replace this with a role specifically for annotations.
      node.role = ax::mojom::Role::kMain;
      return;
    }
  }

  if (!load_complete || !annotator_remote_.is_bound()) {
    return;
  }
  // TODO: This is called several times in a row as the page loads content
  // asynchronously. We should call this once, when the page reaches a stable
  // condition.
  annotator_remote_->ExtractMainNode(
      *update, base::BindOnce(&AXMainNodeAnnotator::ProcessScreen2xResult,
                              weak_ptr_factory_.GetWeakPtr(), document));
}

void AXMainNodeAnnotator::ProcessScreen2xResult(const WebDocument& document,
                                                ui::AXNodeID main_node_id) {
  main_node_id_ = main_node_id;
  if (main_node_id_ == ui::kInvalidAXNodeID) {
    return;
  }
  WebAXObject object =
      WebAXObject::FromWebDocumentByID(document, main_node_id_);
  render_accessibility_->MarkWebAXObjectDirty(object);
}

void AXMainNodeAnnotator::BindAnnotatorForTesting(
    mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor>
        annotator) {
  annotator_remote_.Bind(std::move(annotator));
}

}  // namespace content
