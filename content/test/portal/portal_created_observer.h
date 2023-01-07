// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PORTAL_PORTAL_CREATED_OBSERVER_H_
#define CONTENT_TEST_PORTAL_PORTAL_CREATED_OBSERVER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-forward.h"

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
class PortalCreatedObserver
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
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

  // blink::mojom::LocalFrameHostInterceptorForTesting
  blink::mojom::LocalFrameHost* GetForwardingInterface() override;
  void CreatePortal(
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> portal,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
      blink::mojom::RemoteFrameInterfacesFromRendererPtr
          remote_frame_interfaces,
      CreatePortalCallback callback) override;
  void AdoptPortal(const blink::PortalToken& portal_token,
                   blink::mojom::RemoteFrameInterfacesFromRendererPtr
                       remote_frame_interfaces,
                   AdoptPortalCallback callback) override;

  // Wait until a portal is created (either newly or through adoption).
  Portal* WaitUntilPortalCreated();

 private:
  void DidCreatePortal();

  raw_ptr<RenderFrameHostImpl, DanglingUntriaged> render_frame_host_impl_;
  mojo::test::ScopedSwapImplForTesting<
      mojo::AssociatedReceiver<blink::mojom::LocalFrameHost>>
      swapped_impl_;
  base::OnceCallback<void(Portal*)> created_cb_;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  raw_ptr<Portal, DanglingUntriaged> portal_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_TEST_PORTAL_PORTAL_CREATED_OBSERVER_H_
