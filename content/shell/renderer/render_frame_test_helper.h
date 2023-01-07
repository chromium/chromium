// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_RENDER_FRAME_TEST_HELPER_H_
#define CONTENT_SHELL_RENDERER_RENDER_FRAME_TEST_HELPER_H_

#include "content/public/renderer/render_frame_observer.h"
#include "content/shell/common/render_frame_test_helper.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class RenderFrameTestHelper : public mojom::RenderFrameTestHelper,
                              public RenderFrameObserver {
 public:
  // Creates a new instance that deletes itself when the RenderFrame is
  // destroyed.
  static void Create(
      RenderFrame& render_frame,
      mojo::PendingReceiver<mojom::RenderFrameTestHelper> receiver);

  ~RenderFrameTestHelper() override;

  // mojom::RenderFrameTestHelper overrides:
  void GetDocumentToken(GetDocumentTokenCallback callback) override;

  // RenderFrameObserver overrides:
  void OnDestruct() override;

 private:
  explicit RenderFrameTestHelper(
      RenderFrame& render_frame,
      mojo::PendingReceiver<mojom::RenderFrameTestHelper> receiver);

  const mojo::Receiver<mojom::RenderFrameTestHelper> receiver_;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_RENDER_FRAME_TEST_HELPER_H_
