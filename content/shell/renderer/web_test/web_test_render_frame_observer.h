// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_WEB_TEST_RENDER_FRAME_OBSERVER_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_WEB_TEST_RENDER_FRAME_OBSERVER_H_

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/shell/common/web_test.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {

class WebTestRenderFrameObserver : public RenderFrameObserver,
                                   public mojom::WebTestControl {
 public:
  explicit WebTestRenderFrameObserver(RenderFrame* render_frame);
  ~WebTestRenderFrameObserver() override;

 private:
  // RenderFrameObserver implementation.
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidFailProvisionalLoad() override;
  void OnDestruct() override;

  // mojom::WebTestControl implementation.
  void CaptureDump(CaptureDumpCallback callback) override;
  void CompositeWithRaster(CompositeWithRasterCallback callback) override;
  void DumpFrameLayout(DumpFrameLayoutCallback callback) override;
  void SetTestConfiguration(mojom::ShellTestConfigurationPtr config) override;
  void ReplicateTestConfiguration(
      mojom::ShellTestConfigurationPtr config) override;
  void SetupSecondaryRenderer() override;
  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::WebTestControl> receiver);

  mojo::AssociatedReceiver<mojom::WebTestControl> receiver_{this};
  bool focus_on_next_commit_ = false;
  DISALLOW_COPY_AND_ASSIGN(WebTestRenderFrameObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_WEB_TEST_RENDER_FRAME_OBSERVER_H_
