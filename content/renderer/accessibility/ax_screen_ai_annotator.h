// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_

#include "base/memory/weak_ptr.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace blink {
class WebAXObject;
}

namespace ui {
struct AXNodeData;
}

namespace content {

class AXScreenAIAnnotator {
 public:
  AXScreenAIAnnotator(RenderAccessibilityImpl* const render_accessibility,
                      mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
                          screen_ai_annotator);
  ~AXScreenAIAnnotator();
  AXScreenAIAnnotator(const AXScreenAIAnnotator&) = delete;
  AXScreenAIAnnotator& operator=(const AXScreenAIAnnotator&) = delete;

  // If |src| is already annotated, updates |dst| and returns true. Otherwise
  // returns false.
  bool ApplyAnnotationsIfAvailable(const blink::WebAXObject& src,
                                   ui::AXNodeData& dst);

  // If validated by heuristics, |object|'s image is sent to ScreenAI service.
  void MaybeRunScreenAI(const blink::WebAXObject& object);

 private:
  // Heuristic based assessment if an object needs annotation.
  bool ShouldAnnotateObject(const blink::WebAXObject& object);

  // Receives the annotation from ScreenAI service, stores them in
  // |annotations_|, and marks the respective object dirty.
  void OnAnnotationReceived(ui::AXNodeID ax_id,
                            screen_ai::mojom::ErrorType error_type,
                            std::vector<screen_ai::mojom::NodePtr> annotation);

  std::unordered_map<ui::AXNodeID, std::vector<screen_ai::mojom::Node>>
      annotations_;

  // Weak, owns us.
  RenderAccessibilityImpl* const render_accessibility_;

  mojo::Remote<screen_ai::mojom::ScreenAIAnnotator> screen_ai_annotator_;

  base::WeakPtrFactory<AXScreenAIAnnotator> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_
