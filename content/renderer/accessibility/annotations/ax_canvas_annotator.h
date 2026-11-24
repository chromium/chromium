// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_CANVAS_ANNOTATOR_H_
#define CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_CANVAS_ANNOTATOR_H_

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/accessibility/annotations/ax_annotator.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {

class WebAXContext;
class WebAXObject;
class WebDocument;

}  // namespace blink

namespace content {

class RenderAccessibilityImpl;

// This class gets notified that certain canvases have been added, removed or
// updated on a page. This class is then responsible for retrieving the
// OCR text for all canvases that need accessibility support and notifying the
// RenderAccessibility that owns it to update the relevant canvas annotations.
class CONTENT_EXPORT AXCanvasAnnotator : public AXAnnotator {
 public:
  static constexpr uint32_t kEnableAXMode = ui::AXMode::kScreenReader;

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
  void DidCreateNewDocument() override;
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
    std::optional<size_t> last_bitmap_hash;
  };

  void AddCanvasAnnotationForNode(const blink::WebAXObject& src,
                                  ui::AXNodeData& dst);

  // Evaluates `src` and dispatches an OCR request if needed, or updates the
  // cached state accordingly.
  void RefreshCanvasAnnotation(const blink::WebAXObject& src, int ax_id);

  // Clears Blink's OCR request flag on `src` and removes any cached or pending
  // annotation for `ax_id`.
  void EvictCanvasAnnotation(const blink::WebAXObject& src, int ax_id);

  // Clears Blink's OCR request flag on `src` while retaining the existing
  // cached annotation.
  void KeepCachedCanvasAnnotation(const blink::WebAXObject& src);

  // Stores the completed OCR `text` in `info` and clears its pending state.
  void SetCanvasAnnotation(CanvasAnnotationInfo& info, std::string text);

  // Connects to the OCR service if not already connected.
  // Returns true if connected or successfully bound.
  bool ConnectOcrIfNeeded();

  // Gets called when the OCR service shuts down or disconnects.
  void OnOcrDisconnected();

  // Gets called when a canvas gets annotated by the canvas annotation service.
  void OnCanvasAnnotated(
      int ax_id,
      screen_ai::mojom::VisualAnnotationPtr visual_annotation);

  // Applies or evicts the completed OCR result once layout and AX state are
  // clean.
  void ApplyCanvasAnnotation(
      int ax_id,
      blink::WebAXContext& ax_context,
      const screen_ai::mojom::VisualAnnotation& visual_annotation);

  // Returns the pending annotation entry for `ax_id`, or nullptr if no OCR
  // request is pending for `ax_id`.
  CanvasAnnotationInfo* FindPendingAnnotation(int ax_id);

  // Marks `canvas` dirty and schedules immediate AX serialization if `canvas`
  // is still attached and included in the tree.
  void MarkCanvasDirty(const blink::WebAXObject& canvas,
                       blink::WebAXContext& ax_context);

  void PruneStaleAnnotations(const blink::WebDocument& document);

  // Non-owning reference to the owner of this annotator.
  const raw_ref<RenderAccessibilityImpl> render_accessibility_;

  // A reference to the OCR service.
  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> annotator_remote_;

  // Keeps track of the automatic annotations for each canvas, keyed by
  // AXNodeData::id so entries can still be looked up and erased after an
  // AXObject is detached.
  std::unordered_map<int, CanvasAnnotationInfo> canvas_annotations_;

  bool annotations_enabled_ = false;

  base::WeakPtrFactory<AXCanvasAnnotator> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_CANVAS_ANNOTATOR_H_
