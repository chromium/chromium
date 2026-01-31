// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_handler_web_contents_observer.h"

namespace content {

PaymentHandlerWebContentsObserver::PaymentHandlerWebContentsObserver(
    WebContents* web_contents,
    base::OnceClosure error_callback)
    : WebContentsObserver(web_contents),
      error_callback_(std::move(error_callback)) {}

PaymentHandlerWebContentsObserver::~PaymentHandlerWebContentsObserver() =
    default;

void PaymentHandlerWebContentsObserver::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (error_callback_) {
    std::move(error_callback_).Run();
  }
}
void PaymentHandlerWebContentsObserver::OnRendererUnresponsive(
    RenderProcessHost* render_process_host) {
  if (error_callback_) {
    std::move(error_callback_).Run();
  }
}

}  // namespace content
