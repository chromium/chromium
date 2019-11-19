// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FRAME_HOST_INTERCEPTOR_H_
#define CONTENT_TEST_FRAME_HOST_INTERCEPTOR_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

class RenderFrameHost;

// Allows intercepting calls to mojom::FrameHost (e.g. BeginNavigation) just
// before they are dispatched to the implementation. This enables unit/browser
// tests to scrutinize/alter the parameters, or simulate race conditions by
// triggering other calls just before dispatching the original call.
//
// NOTE: DidCommitProvisionalLoad is handled separately, because it is in a
// transient state right now, and is soon going away from mojom::FrameHost.
class FrameHostInterceptor : public WebContentsObserver {
 public:
  // Constructs an instance that will intercept FrameHost calls in any frame of
  // the |web_contents| while the instance is in scope.
  explicit FrameHostInterceptor(WebContents* web_contents);
  ~FrameHostInterceptor() override;

  // Called just before BeginNavigation IPC would be dispatched to
  // |render_frame_host|.
  //
  // Return false to cancel the dispatching of this message.
  //
  // Return true (and/or modify args as needed) to dispatch this message to the
  // original implementation.
  //
  // By default this method returns true (e.g. doesn't do anything to the
  // original messages and just forwards them to the original implementation).
  virtual bool WillDispatchBeginNavigation(
      RenderFrameHost* render_frame_host,
      mojom::CommonNavigationParamsPtr* common_params,
      mojom::BeginNavigationParamsPtr* begin_params,
      mojo::PendingRemote<blink::mojom::BlobURLToken>* blob_url_token,
      mojo::PendingAssociatedRemote<mojom::NavigationClient>* navigation_client,
      mojo::PendingRemote<blink::mojom::NavigationInitiator>*
          navigation_initiator);

 private:
  class FrameAgent;

  // WebContentsObserver:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

  std::map<RenderFrameHost*, std::unique_ptr<FrameAgent>> frame_agents_;

  DISALLOW_COPY_AND_ASSIGN(FrameHostInterceptor);
};

}  // namespace content

#endif  // CONTENT_TEST_FRAME_HOST_INTERCEPTOR_H_
