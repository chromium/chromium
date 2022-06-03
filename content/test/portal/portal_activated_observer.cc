// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/portal/portal_activated_observer.h"

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

PortalActivatedObserver::PortalActivatedObserver(Portal* portal)
    : interceptor_(PortalInterceptorForTesting::From(portal)->GetWeakPtr()) {
  interceptor_->AddObserver(this);
}

PortalActivatedObserver::~PortalActivatedObserver() {
  if (auto* interceptor = interceptor_.get())
    interceptor->RemoveObserver(this);
}

void PortalActivatedObserver::WaitForActivate() {
  if (has_activated_)
    return;

  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  run_loop.Run();

  DCHECK(has_activated_);
}

blink::mojom::PortalActivateResult
PortalActivatedObserver::WaitForActivateResult() {
  WaitForActivate();
  if (result_)
    return *result_;

  base::RunLoop run_loop;
  base::AutoReset<base::RunLoop*> auto_reset(&run_loop_, &run_loop);
  run_loop.Run();

  DCHECK(result_);
  return *result_;
}

void PortalActivatedObserver::OnPortalActivate() {
  DCHECK(!has_activated_)
      << "PortalActivatedObserver can't handle overlapping activations.";
  has_activated_ = true;

  if (run_loop_)
    run_loop_->Quit();
}

void PortalActivatedObserver::OnPortalActivateResult(
    blink::mojom::PortalActivateResult result) {
  DCHECK(has_activated_) << "PortalActivatedObserver should observe the whole "
                            "activation; this may be a race.";
  DCHECK(!result_)
      << "PortalActivatedObserver can't handle overlapping activations.";
  result_ = result;

  if (run_loop_)
    run_loop_->Quit();

  if (auto* interceptor = interceptor_.get())
    interceptor->RemoveObserver(this);
}

}  // namespace content
