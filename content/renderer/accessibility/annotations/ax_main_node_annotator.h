// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_MAIN_NODE_ANNOTATOR_H_
#define CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_MAIN_NODE_ANNOTATOR_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/accessibility/annotations/ax_annotator.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace blink {
class WebDocument;
}  // namespace blink

namespace content {

class CONTENT_EXPORT AXMainNodeAnnotator : public AXAnnotator {
 public:
  enum AXMainNodeAnnotatorAuthorStatus {
    kUnconfirmed = 0,
    kAuthorProvidedMain = 1,
    kAuthorDidNotProvideMain = 2,
    kMaxValue = kUnconfirmed
  };

  explicit AXMainNodeAnnotator(
      RenderAccessibilityImpl* const render_accessibility);
  AXMainNodeAnnotator(const AXMainNodeAnnotator&) = delete;
  AXMainNodeAnnotator& operator=(const AXMainNodeAnnotator&) = delete;
  ~AXMainNodeAnnotator() override;

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
      const std::vector<ui::AXTreeUpdate>& updates) override {}

  void BindAnnotatorForTesting(
      mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor>
          annotator);

 private:
  void ProcessScreen2xResult(const blink::WebDocument& document,
                             ui::AXNodeID main_node_id);

  void ComputeAuthorStatus(ui::AXTreeUpdate* update);

  // Weak, owns us.
  const raw_ptr<RenderAccessibilityImpl> render_accessibility_;

  bool annotator_enabled_ = false;

  // The remote of the Screen2x main content extractor. The receiver lives in
  // the utility process.
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      annotator_remote_;

  // The id of the main node identified.
  ui::AXNodeID main_node_id_ = ui::kInvalidAXNodeID;

  // Whether a main node was provided by the author.
  AXMainNodeAnnotatorAuthorStatus author_status_ =
      AXMainNodeAnnotatorAuthorStatus::kUnconfirmed;

  base::WeakPtrFactory<AXMainNodeAnnotator> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_MAIN_NODE_ANNOTATOR_H_
