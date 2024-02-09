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

  ax_annotators_.emplace_back(
      std::make_unique<AXImageAnnotator>(render_accessibility_));
}

AXAnnotatorsManager::~AXAnnotatorsManager() {}

void AXAnnotatorsManager::Annotate(const blink::WebDocument& document,
                                   ui::AXTreeUpdate* update,
                                   bool load_complete) {
  for (const auto& annotator : ax_annotators_) {
    annotator->Annotate(document, update, load_complete);
  }
}

void AXAnnotatorsManager::AccessibilityModeChanged(ui::AXMode old_mode,
                                                   ui::AXMode new_mode) {
  for (const auto& annotator : ax_annotators_) {
    uint32_t flag = annotator->GetAXModeToEnableAnnotations();
    if (!old_mode.has_mode(flag) && new_mode.has_mode(flag)) {
      annotator->EnableAnnotations();
    } else if (old_mode.has_mode(flag) && !new_mode.has_mode(flag)) {
      annotator->CancelAnnotations();
    }
  }
}

void AXAnnotatorsManager::CancelAnnotations() {
  for (const auto& annotator : ax_annotators_) {
    annotator->CancelAnnotations();
  }
}

void AXAnnotatorsManager::PerformAction(ax::mojom::Action action) {
  bool applied_annotations = false;
  for (const auto& annotator : ax_annotators_) {
    if (action != annotator->GetAXActionToEnableAnnotations()) {
      continue;
    }
    applied_annotations = true;
    annotator->EnableAnnotations();
  }
  if (!applied_annotations) {
    return;
  }
  // Rebuild the document tree so that annotations are applied.
  DCHECK(render_accessibility_->GetAXContext());
  render_accessibility_->GetAXContext()->MarkDocumentDirty();
}

void AXAnnotatorsManager::AddDebuggingAttributes(
    const std::vector<ui::AXTreeUpdate>& updates) {
  for (const auto& annotator : ax_annotators_) {
    annotator->AddDebuggingAttributes(updates);
  }
}

void AXAnnotatorsManager::AddAnnotatorForTesting(
    std::unique_ptr<AXAnnotator> annotator) {
  ax_annotators_.push_back(std::move(annotator));
}

void AXAnnotatorsManager::ClearAnnotatorsForTesting() {
  ax_annotators_.clear();
}

}  // namespace content
