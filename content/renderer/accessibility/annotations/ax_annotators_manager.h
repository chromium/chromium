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
class WebAXObject;
class WebDocument;
}  // namespace blink

namespace content {
class AXImageAnnotator;
class RenderAccessibilityImpl;

class CONTENT_EXPORT AXAnnotatorsManager {
 public:
  explicit AXAnnotatorsManager(
      RenderAccessibilityImpl* const render_accessibility);
  AXAnnotatorsManager(const AXAnnotatorsManager&) = delete;
  AXAnnotatorsManager& operator=(const AXAnnotatorsManager&) = delete;
  ~AXAnnotatorsManager();

  void set_has_injected_stylesheet(bool has_injected_stylesheet) {
    has_injected_stylesheet_ = has_injected_stylesheet;
  }

  void Annotate(const blink::WebDocument& document, ui::AXTreeUpdate* update);

  // Update AXAnnotators based on a changed accessibility mode.
  void AccessibilityModeChanged(ui::AXMode old_mode, ui::AXMode new_mode);

  void CancelAnnotations();

  void PerformAction(ax::mojom::Action action);

  void AddImageAnnotationDebuggingAttributes(
      const std::vector<ui::AXTreeUpdate>& updates);

  static void IgnoreProtocolChecksForTesting();

 private:
  friend class AXImageAnnotatorTest;

  // Creates and takes ownership of an instance of the class that automatically
  // labels images for accessibility.
  void CreateAXImageAnnotator();

  void AddImageAnnotations(const blink::WebDocument& document,
                           ui::AXTreeUpdate* update);
  void AddImageAnnotationsForNode(blink::WebAXObject& src, ui::AXNodeData* dst);

  // The RenderAccessibilityManager that owns us.
  raw_ptr<RenderAccessibilityImpl, ExperimentalRenderer> render_accessibility_;

  // Manages the automatic image annotations, if enabled.
  std::unique_ptr<AXImageAnnotator> ax_image_annotator_;

  // Whether or not we've injected a stylesheet in this document
  // (only when debugging flags are enabled, never under normal circumstances).
  bool has_injected_stylesheet_ = false;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_ANNOTATORS_MANAGER_H_
