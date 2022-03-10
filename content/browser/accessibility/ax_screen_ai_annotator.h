// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_
#define CONTENT_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace gfx {
class Image;
}

namespace content {

class RenderFrameHost;

class AXScreenAIAnnotator {
 public:
  AXScreenAIAnnotator(
      RenderFrameHost* const render_frame_host,
      mojo::AssociatedRemote<screen_ai::mojom::ScreenAIAnnotator>
          screen_ai_annotator);
  ~AXScreenAIAnnotator();
  AXScreenAIAnnotator(const AXScreenAIAnnotator&) = delete;
  AXScreenAIAnnotator& operator=(const AXScreenAIAnnotator&) = delete;

  // Takes a screenshot and sends it to |OnScreenshotReceived| through an async
  // call.
  void Run();

 private:
  // Receives an screenshot and sends it to ScreenAI library for processing.
  void OnScreenshotReceived(gfx::Image snapshot);

  // Receives the annotation from ScreenAI service, sends it to
  // |render_frame_host_| as an accessibility action.
  void OnAnnotationReceived(screen_ai::mojom::ErrorType error_type,
                            std::vector<screen_ai::mojom::NodePtr> annotation);

  // Owns us.
  raw_ptr<RenderFrameHost> const render_frame_host_;

  mojo::AssociatedRemote<screen_ai::mojom::ScreenAIAnnotator>
      screen_ai_annotator_;

  base::WeakPtrFactory<AXScreenAIAnnotator> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_AX_SCREEN_AI_ANNOTATOR_H_
