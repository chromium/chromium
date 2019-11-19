// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/frame_host_interceptor.h"

#include <utility>

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace content {

// Responsible for intercepting mojom::FrameHost messages being disptached to
// a given RenderFrameHostImpl.
class FrameHostInterceptor::FrameAgent
    : public mojom::FrameHostInterceptorForTesting {
 public:
  FrameAgent(FrameHostInterceptor* interceptor, RenderFrameHost* rfh)
      : interceptor_(interceptor),
        rfhi_(static_cast<RenderFrameHostImpl*>(rfh)),
        impl_(receiver().SwapImplForTesting(this)) {}

  ~FrameAgent() override {
    auto* old_impl = receiver().SwapImplForTesting(impl_);
    // TODO(https://crbug.com/729021): Investigate the scenario where
    // |old_impl| can be nullptr if the renderer process is killed.
    DCHECK_EQ(this, old_impl);
  }

 protected:
  mojo::AssociatedReceiver<mojom::FrameHost>& receiver() {
    return rfhi_->frame_host_receiver_for_testing();
  }

  // mojom::FrameHostInterceptorForTesting:
  FrameHost* GetForwardingInterface() override { return impl_; }

  void BeginNavigation(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>
          navigation_initiator) override {
    if (interceptor_->WillDispatchBeginNavigation(
            rfhi_, &common_params, &begin_params, &blob_url_token,
            &navigation_client, &navigation_initiator)) {
      GetForwardingInterface()->BeginNavigation(
          std::move(common_params), std::move(begin_params),
          std::move(blob_url_token), std::move(navigation_client),
          std::move(navigation_initiator));
    }
  }

 private:
  FrameHostInterceptor* interceptor_;

  RenderFrameHostImpl* rfhi_;
  mojom::FrameHost* impl_;

  DISALLOW_COPY_AND_ASSIGN(FrameAgent);
};

FrameHostInterceptor::FrameHostInterceptor(WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  for (auto* rfh : web_contents->GetAllFrames()) {
    if (rfh->IsRenderFrameLive())
      RenderFrameCreated(rfh);
  }
}

FrameHostInterceptor::~FrameHostInterceptor() = default;

bool FrameHostInterceptor::WillDispatchBeginNavigation(
    RenderFrameHost* render_frame_host,
    mojom::CommonNavigationParamsPtr* common_params,
    mojom::BeginNavigationParamsPtr* begin_params,
    mojo::PendingRemote<blink::mojom::BlobURLToken>* blob_url_token,
    mojo::PendingAssociatedRemote<mojom::NavigationClient>* navigation_client,
    mojo::PendingRemote<blink::mojom::NavigationInitiator>*
        navigation_initiator) {
  return true;
}

void FrameHostInterceptor::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  bool did_insert;
  std::tie(std::ignore, did_insert) = frame_agents_.emplace(
      render_frame_host, std::make_unique<FrameAgent>(this, render_frame_host));
  DCHECK(did_insert);
}

void FrameHostInterceptor::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  bool did_remove = !!frame_agents_.erase(render_frame_host);
  DCHECK(did_remove);
}

}  // namespace content
