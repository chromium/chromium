// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_CANVAS_ANNOTATOR_H_
#define CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_CANVAS_ANNOTATOR_H_

#include <optional>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "content/renderer/accessibility/annotations/ax_annotator.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/blink/public/web/web_document.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace blink {

class WebAXObject;

}  // namespace blink

namespace content {

class RenderAccessibilityImpl;

// This class gets notified that certain canvases have been added, removed or
// updated on a page. This class is then responsible for retrieving the
// OCR text for all canvases that need accessibility support and notifying the
// RenderAccessibility that owns it to update the relevant canvas annotations.
class CONTENT_EXPORT AXCanvasAnnotator : public AXAnnotator,
                                         public base::CheckedObserver {
 public:
  // `render_accessibility` is the owner of this class and should outlive it.
  explicit AXCanvasAnnotator(
      RenderAccessibilityImpl* const render_accessibility);
  AXCanvasAnnotator(const AXCanvasAnnotator&) = delete;
  AXCanvasAnnotator& operator=(const AXCanvasAnnotator&) = delete;
  ~AXCanvasAnnotator() override;

  // AXAnnotator:
  void Annotate(const blink::WebDocument& document,
                ui::AXTreeUpdate* update,
                bool load_complete) override;
  void EnableAnnotations() override;
  void CancelAnnotations() override;
  uint32_t GetAXModeToEnableAnnotations() override;
  bool HasAXActionToEnableAnnotations() override;
  ax::mojom::Action GetAXActionToEnableAnnotations() override;
  void AddDebuggingAttributes(
      const std::vector<ui::AXTreeUpdate>& updates) override;

 private:
  struct CanvasAnnotationInfo {
    std::string text;
    bool is_pending = false;
  };

  void AddCanvasAnnotationForNode(blink::WebAXObject& src, ui::AXNodeData& dst);

  // Connects to the Screen AI OCR service if not already connected.
  void ConnectOcrIfNeeded();

  // Gets called when the OCR service shuts down or disconnects.
  void OnOcrDisconnected();

  // Gets called when a canvas gets annotated by the canvas annotation service.
  void OnCanvasAnnotated(
      const blink::WebAXObject& canvas,
      screen_ai::mojom::VisualAnnotationPtr visual_annotation);

  void PruneStaleAnnotations(const blink::WebDocument& document);

  // Weak, owns us.
  const raw_ref<RenderAccessibilityImpl> render_accessibility_;

  // A reference to the Screen AI service for OCR.
  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> annotator_remote_;

  // Keeps track of the automatic annotations for each canvas.
  // The key is retrieved using WebAXObject::AxID().
  std::unordered_map<int, CanvasAnnotationInfo> canvas_annotations_;

  // Document to which the current annotations belong.
  blink::WebDocument current_document_;

  // Number of tree updates processed since the last pruning.
  int updates_since_last_pruning_ = 0;

  bool annotations_enabled_ = false;

  base::WeakPtrFactory<AXCanvasAnnotator> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_CANVAS_ANNOTATOR_H_
