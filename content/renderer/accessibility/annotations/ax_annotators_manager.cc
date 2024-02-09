// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_annotators_manager.h"

#include <utility>

#include "content/renderer/accessibility/annotations/ax_image_annotator.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/web/web_document.h"

namespace content {

AXAnnotatorsManager::AXAnnotatorsManager(
    RenderAccessibilityImpl* const render_accessibility)
    : render_accessibility_(render_accessibility) {
  DCHECK(render_accessibility_);

  ax_image_annotator_ =
      std::make_unique<AXImageAnnotator>(render_accessibility_);
}

AXAnnotatorsManager::~AXAnnotatorsManager() {}

void AXAnnotatorsManager::Annotate(const blink::WebDocument& document,
                                   ui::AXTreeUpdate* update,
                                   bool load_complete) {
  ax_image_annotator_->Annotate(document, update, load_complete);
}

void AXAnnotatorsManager::AccessibilityModeChanged(ui::AXMode old_mode,
                                                   ui::AXMode new_mode) {
  uint32_t flag = ax_image_annotator_->GetAXModeToEnableAnnotations();
  if (!old_mode.has_mode(flag) && new_mode.has_mode(flag)) {
    ax_image_annotator_->EnableAnnotations();
  } else if (old_mode.has_mode(flag) && !new_mode.has_mode(flag)) {
    ax_image_annotator_->CancelAnnotations();
  }
}

void AXAnnotatorsManager::CancelAnnotations() {
  ax_image_annotator_->CancelAnnotations();
}

void AXAnnotatorsManager::PerformAction(ax::mojom::Action action) {
  if (action != ax_image_annotator_->GetAXActionToEnableAnnotations()) {
    return;
  }
  ax_image_annotator_->EnableAnnotations();

  // Rebuild the document tree so that annotations are applied.
  DCHECK(render_accessibility_->GetAXContext());
  render_accessibility_->GetAXContext()->MarkDocumentDirty();
}

void AXAnnotatorsManager::AddDebuggingAttributes(
    const std::vector<ui::AXTreeUpdate>& updates) {
  ax_image_annotator_->AddDebuggingAttributes(updates);
}

}  // namespace content
