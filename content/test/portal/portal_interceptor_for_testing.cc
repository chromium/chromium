// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/portal/portal_interceptor_for_testing.h"

#include <utility>
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"

namespace content {

// static
PortalInterceptorForTesting* PortalInterceptorForTesting::Create(
    RenderFrameHostImpl* render_frame_host_impl,
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client) {
  auto portal = std::make_unique<content::Portal>(render_frame_host_impl);
  portal->Bind(std::move(receiver), std::move(client));
  auto* portal_raw = portal.get();
  render_frame_host_impl->OnPortalCreatedForTesting(std::move(portal));
  return Create(render_frame_host_impl, portal_raw);
}

PortalInterceptorForTesting* PortalInterceptorForTesting::Create(
    RenderFrameHostImpl* render_frame_host_impl,
    content::Portal* portal) {
  auto interceptor = base::WrapUnique(
      new PortalInterceptorForTesting(render_frame_host_impl, portal));
  auto* raw_interceptor = interceptor.get();
  // TODO(wfh): determine how to handle this for portal interceptors.
  std::ignore = portal->SetInterceptorForTesting(std::move(interceptor));
  return raw_interceptor;
}

// static
PortalInterceptorForTesting* PortalInterceptorForTesting::From(
    content::Portal* portal) {
  blink::mojom::Portal* impl = portal->GetInterceptorForTesting();
  CHECK_NE(static_cast<blink::mojom::Portal*>(portal), impl);
  auto* interceptor = static_cast<PortalInterceptorForTesting*>(impl);
  CHECK_EQ(interceptor->GetPortal(), portal);
  return interceptor;
}

PortalInterceptorForTesting::PortalInterceptorForTesting(
    RenderFrameHostImpl* render_frame_host_impl,
    content::Portal* portal)
    : observers_(base::MakeRefCounted<
                 base::RefCountedData<base::ObserverList<Observer>>>()),
      portal_(portal) {
  DCHECK_EQ(render_frame_host_impl, portal->owner_render_frame_host());
}

PortalInterceptorForTesting::~PortalInterceptorForTesting() = default;

blink::mojom::Portal* PortalInterceptorForTesting::GetForwardingInterface() {
  return portal_;
}

void PortalInterceptorForTesting::Activate(blink::TransferableMessage data,
                                           base::TimeTicks activation_time,
                                           uint64_t trace_id,
                                           ActivateCallback callback) {
  for (Observer& observer : observers_->data)
    observer.OnPortalActivate();

  // |this| can be destroyed after Activate() is called.
  portal_->Activate(
      std::move(data), activation_time, trace_id,
      base::BindOnce(
          [](const scoped_refptr<
                 base::RefCountedData<base::ObserverList<Observer>>>& observers,
             ActivateCallback callback,
             blink::mojom::PortalActivateResult result) {
            for (Observer& observer : observers->data)
              observer.OnPortalActivateResult(result);
            std::move(callback).Run(result);
          },
          observers_, std::move(callback)));
}

void PortalInterceptorForTesting::Navigate(
    const GURL& url,
    blink::mojom::ReferrerPtr referrer,
    blink::mojom::Portal::NavigateCallback callback) {
  if (navigate_callback_) {
    navigate_callback_.Run(url, std::move(referrer), std::move(callback));
    return;
  }

  portal_->Navigate(url, std::move(referrer), std::move(callback));
}

}  // namespace content
