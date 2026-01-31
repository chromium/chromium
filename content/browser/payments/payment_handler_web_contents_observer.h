// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_HANDLER_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_HANDLER_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// Observes a WebContents instance used by a payment handler to detect
// renderer process crash and unresponsiveness.
class CONTENT_EXPORT PaymentHandlerWebContentsObserver final
    : public WebContentsObserver {
 public:
  PaymentHandlerWebContentsObserver(WebContents* web_contents,
                                    base::OnceClosure error_callback);
  ~PaymentHandlerWebContentsObserver() override;

  // WebContentsObserver implementation.
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void OnRendererUnresponsive(RenderProcessHost* render_process_host) override;

 private:
  base::OnceClosure error_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_HANDLER_WEB_CONTENTS_OBSERVER_H_
