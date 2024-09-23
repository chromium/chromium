// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_ANNOTATORS_MANAGER_H_
#define CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_ANNOTATORS_MANAGER_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace blink {
class WebDocument;
}  // namespace blink

namespace content {
class AXAnnotator;
class RenderAccessibilityImpl;

class CONTENT_EXPORT AXAnnotatorsManager {
 public:
  explicit AXAnnotatorsManager(
      RenderAccessibilityImpl* const render_accessibility);
  AXAnnotatorsManager(const AXAnnotatorsManager&) = delete;
  AXAnnotatorsManager& operator=(const AXAnnotatorsManager&) = delete;
  ~AXAnnotatorsManager();

  // Annotate the document with the given updates. |load_complete| is a boolean
  // denoting whether this annotate call was the result of a load complete.
  void Annotate(const blink::WebDocument& document,
                ui::AXTreeUpdate* update,
                bool load_complete);

  // Cancel any in-progress annotations.
  void CancelAnnotations();

  // Enables annotations if the accessibility mode for this feature is turned
  // on, otherwise cancels annotations.
  void AccessibilityModeChanged(ui::AXMode old_mode, ui::AXMode new_mode);

  // Enables annotations if the action for this feature is fired.
  void PerformAction(ax::mojom::Action action);

  // Add any additional debugging attributes.
  void AddDebuggingAttributes(const std::vector<ui::AXTreeUpdate>& updates);

  void AddAnnotatorForTesting(std::unique_ptr<AXAnnotator>);
  void ClearAnnotatorsForTesting();

 private:
  // The RenderAccessibilityManager that owns us.
  raw_ptr<RenderAccessibilityImpl> render_accessibility_;

  std::vector<std::unique_ptr<AXAnnotator>> ax_annotators_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_ANNOTATORS_MANAGER_H_
