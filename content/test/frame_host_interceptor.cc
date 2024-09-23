// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/frame_host_interceptor.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

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

  FrameAgent(const FrameAgent&) = delete;
  FrameAgent& operator=(const FrameAgent&) = delete;

  ~FrameAgent() override {
    auto* old_impl = receiver().SwapImplForTesting(impl_);
    // TODO(crbug.com/40523839): Investigate the scenario where
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
      blink::mojom::CommonNavigationParamsPtr common_params,
      blink::mojom::BeginNavigationParamsPtr begin_params,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationStateKeepAliveHandle>
          initiator_navigation_state_keep_alive_handle,
      mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
          renderer_cancellation_listener) override {
    if (interceptor_->WillDispatchBeginNavigation(
            rfhi_, &common_params, &begin_params, &blob_url_token,
            &navigation_client)) {
      GetForwardingInterface()->BeginNavigation(
          std::move(common_params), std::move(begin_params),
          std::move(blob_url_token), std::move(navigation_client),
          std::move(initiator_navigation_state_keep_alive_handle),
          std::move(renderer_cancellation_listener));
    }
  }

 private:
  raw_ptr<FrameHostInterceptor> interceptor_;

  raw_ptr<RenderFrameHostImpl> rfhi_;
  raw_ptr<mojom::FrameHost> impl_;
};

FrameHostInterceptor::FrameHostInterceptor(WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  web_contents->ForEachRenderFrameHost(
      [this](RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive())
          RenderFrameCreated(render_frame_host);
      });
}

FrameHostInterceptor::~FrameHostInterceptor() = default;

bool FrameHostInterceptor::WillDispatchBeginNavigation(
    RenderFrameHost* render_frame_host,
    blink::mojom::CommonNavigationParamsPtr* common_params,
    blink::mojom::BeginNavigationParamsPtr* begin_params,
    mojo::PendingRemote<blink::mojom::BlobURLToken>* blob_url_token,
    mojo::PendingAssociatedRemote<mojom::NavigationClient>* navigation_client) {
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
