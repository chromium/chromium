// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_PORTAL_PORTAL_ACTIVATED_OBSERVER_H_
#define CONTENT_TEST_PORTAL_PORTAL_ACTIVATED_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/test/portal/portal_interceptor_for_testing.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-forward.h"

namespace base {
class RunLoop;
}  // namespace base

namespace content {

class Portal;

// Allows callers (in tests only) to observe when a portal is activated. This
// portal must be monitored by a PortalInterceptorForTesting (e.g. its creation
// must be observed by PortalCreatedObserver).
//
// This class must be created before activation occurs, and can be used to wait
// for the activate message being received in the browser process, the reply
// from the activated renderer, or both.
class PortalActivatedObserver : public PortalInterceptorForTesting::Observer {
 public:
  explicit PortalActivatedObserver(Portal* portal);
  ~PortalActivatedObserver() override;

  PortalActivatedObserver(const PortalActivatedObserver&) = delete;
  PortalActivatedObserver& operator=(const PortalActivatedObserver&) = delete;

  bool has_activated() const { return has_activated_; }
  blink::mojom::PortalActivateResult result() const { return *result_; }

  // Waits for the Activate method to be called by the predecessor renderer.
  void WaitForActivate();

  // Waits for the computed result of activation, including the handling of the
  // portalactivate event. The content::Portal may have been deleted at this
  // point.
  blink::mojom::PortalActivateResult WaitForActivateResult();

 private:
  // PortalInterceptorForTesting::Observer:
  void OnPortalActivate() override;
  void OnPortalActivateResult(
      blink::mojom::PortalActivateResult result) override;

  const base::WeakPtr<PortalInterceptorForTesting> interceptor_;
  bool has_activated_ = false;
  base::Optional<blink::mojom::PortalActivateResult> result_;
  base::RunLoop* run_loop_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_TEST_PORTAL_PORTAL_ACTIVATED_OBSERVER_H_
