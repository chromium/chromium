// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PORTAL_PORTAL_CREATED_OBSERVER_H_
#define CONTENT_TEST_PORTAL_PORTAL_CREATED_OBSERVER_H_

#include "base/callback.h"
#include "content/common/frame.mojom-test-utils.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"

namespace base {
class RunLoop;
}  // namespace base

namespace content {

class Portal;
class RenderFrameHostImpl;

// The PortalCreatedObserver observes portal creations on
// |render_frame_host_impl|. This observer can be used to monitor for multiple
// Portal creations on the same RenderFrameHost, by repeatedly calling
// WaitUntilPortalCreated().
class PortalCreatedObserver : public mojom::FrameHostInterceptorForTesting {
 public:
  explicit PortalCreatedObserver(RenderFrameHostImpl* render_frame_host_impl);
  ~PortalCreatedObserver() override;

  // If set, callback will be run immediately when the portal is created, before
  // any subsequent tasks which may run before the run loop quits -- or even
  // before it starts, if multiple events are being waited for one after
  // another.
  void set_created_callback(base::OnceCallback<void(Portal*)> created_cb) {
    DCHECK(!portal_) << "Too late to register a created callback.";
    created_cb_ = std::move(created_cb);
  }

  // mojom::FrameHostInterceptorForTesting
  mojom::FrameHost* GetForwardingInterface() override;
  void CreatePortal(
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> portal,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
      CreatePortalCallback callback) override;
  void AdoptPortal(const base::UnguessableToken& portal_token,
                   AdoptPortalCallback callback) override;

  // Wait until a portal is created (either newly or through adoption).
  Portal* WaitUntilPortalCreated();

 private:
  void DidCreatePortal();

  RenderFrameHostImpl* render_frame_host_impl_;
  mojom::FrameHost* old_impl_;
  base::OnceCallback<void(Portal*)> created_cb_;
  base::RunLoop* run_loop_ = nullptr;
  Portal* portal_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_TEST_PORTAL_PORTAL_CREATED_OBSERVER_H_
