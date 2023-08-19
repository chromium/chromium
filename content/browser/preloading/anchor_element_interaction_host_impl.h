// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_ANCHOR_ELEMENT_INTERACTION_HOST_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_ANCHOR_ELEMENT_INTERACTION_HOST_IMPL_H_

#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom.h"

namespace content {

// Receiver for user interaction events for anchor elements. It will then
// forward the events to PreloadingDecider to be processed to trigger a
// preloading action if required.
class CONTENT_EXPORT AnchorElementInteractionHostImpl
    : public DocumentService<blink::mojom::AnchorElementInteractionHost> {
 public:
  // Creates and binds an instance of this class per-frame
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost>
          receiver);

 private:
  AnchorElementInteractionHostImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost>
          receiver);
  // blink::mojom::AnchorElementInteractionHost:
  void OnPointerDown(const GURL& target) override;
  void OnPointerHover(
      const GURL& target,
      blink::mojom::AnchorElementPointerDataPtr mouse_data) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_ANCHOR_ELEMENT_INTERACTION_HOST_IMPL_H_
