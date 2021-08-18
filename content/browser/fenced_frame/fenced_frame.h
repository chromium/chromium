// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_

#include <memory>
#include <string>

#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"

namespace content {

// This is the browser-side host object for the <fencedframe> element
// implemented in Blink. This is only used for the MPArch version of fenced
// frames, not the ShadowDOM implementation. It is owned by and stored directly
// on `RenderFrameHostImpl` for now.
class CONTENT_EXPORT FencedFrame : public blink::mojom::FencedFrameOwnerHost {
 public:
  FencedFrame();
  ~FencedFrame() override;

  void Bind(mojo::PendingAssociatedReceiver<blink::mojom::FencedFrameOwnerHost>
                receiver) {
    receiver_.Bind(std::move(receiver));
  }

 private:
  // Receives messages from the frame owner element in Blink.
  mojo::AssociatedReceiver<blink::mojom::FencedFrameOwnerHost> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_H_
