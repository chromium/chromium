// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_RENDERER_WEB_PAGE_METADATA_AGENT_H_
#define COMPONENTS_WEBAPPS_RENDERER_WEB_PAGE_METADATA_AGENT_H_

#include "build/build_config.h"
#include "components/webapps/common/web_page_metadata_agent.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace webapps {

// This class holds the WebApps specific parts of RenderFrame, and has the same
// lifetime.
class WebPageMetadataAgent : public content::RenderFrameObserver,
                             public mojom::WebPageMetadataAgent {
 public:
  explicit WebPageMetadataAgent(content::RenderFrame* render_frame);
  WebPageMetadataAgent(const WebPageMetadataAgent&) = delete;
  WebPageMetadataAgent& operator=(const WebPageMetadataAgent&) = delete;
  ~WebPageMetadataAgent() override;

 private:
  // RenderFrameObserver implementation.
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void OnDestruct() override;

  // webapps::mojom::WebPageMetadataAgent:
  void GetWebPageMetadata(GetWebPageMetadataCallback callback) override;

  void OnRenderFrameObserverRequest(
      mojo::PendingAssociatedReceiver<mojom::WebPageMetadataAgent> receiver);

  mojo::AssociatedReceiverSet<mojom::WebPageMetadataAgent> receivers_;

  service_manager::BinderRegistry registry_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_RENDERER_WEB_PAGE_METADATA_AGENT_H_
