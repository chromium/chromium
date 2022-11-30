// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PORTAL_PORTAL_INTERCEPTOR_FOR_TESTING_H_
#define CONTENT_TEST_PORTAL_PORTAL_INTERCEPTOR_FOR_TESTING_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "content/browser/portal/portal.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-forward.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-test-utils.h"

namespace content {

class RenderFrameHostImpl;

// The PortalInterceptorForTesting can be used in tests to inspect Portal IPCs.
//
// When in use, it is owned by the target Portal and replaces it in the
// associated binding. It will be destructed when the Portal is; a weak pointer
// should be used to determine whether this has happened.
class PortalInterceptorForTesting final
    : public blink::mojom::PortalInterceptorForTesting {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPortalActivate() {}
    virtual void OnPortalActivateResult(
        blink::mojom::PortalActivateResult result) {}
  };

  static PortalInterceptorForTesting* Create(
      RenderFrameHostImpl* render_frame_host_impl,
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client);
  static PortalInterceptorForTesting* Create(
      RenderFrameHostImpl* render_frame_host_impl,
      content::Portal* portal);
  static PortalInterceptorForTesting* From(content::Portal* portal);

  ~PortalInterceptorForTesting() override;

  // blink::mojom::PortalInterceptorForTesting
  blink::mojom::Portal* GetForwardingInterface() override;
  void Activate(blink::TransferableMessage data,
                base::TimeTicks activation_time,
                uint64_t trace_id,
                ActivateCallback callback) override;
  void Navigate(const GURL& url,
                blink::mojom::ReferrerPtr referrer,
                blink::mojom::Portal::NavigateCallback callback) override;

  // If set, will be used to replace the implementation of Navigate.
  using NavigateCallback =
      base::RepeatingCallback<void(const GURL&,
                                   blink::mojom::ReferrerPtr,
                                   blink::mojom::Portal::NavigateCallback)>;
  void SetNavigateCallback(NavigateCallback callback) {
    navigate_callback_ = std::move(callback);
  }

  // Test getters.
  content::Portal* GetPortal() { return portal_; }
  WebContentsImpl* GetPortalContents() { return portal_->GetPortalContents(); }

  // Useful in observing the intercepted activity.
  base::WeakPtr<PortalInterceptorForTesting> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  void AddObserver(Observer* observer) {
    observers_->data.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) {
    observers_->data.RemoveObserver(observer);
  }

 private:
  PortalInterceptorForTesting(RenderFrameHostImpl* render_frame_host_impl,
                              content::Portal* portal);

  const scoped_refptr<base::RefCountedData<base::ObserverList<Observer>>>
      observers_;
  raw_ptr<content::Portal> portal_;  // Owns this.
  NavigateCallback navigate_callback_;
  base::WeakPtrFactory<PortalInterceptorForTesting> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_TEST_PORTAL_PORTAL_INTERCEPTOR_FOR_TESTING_H_
