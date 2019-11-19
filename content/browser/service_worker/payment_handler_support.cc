// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/payment_handler_support.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

// An instance of this class is created and passed ownership into
// ContentBrowserClient::ShowPaymentHandlerWindow(), to handle these 2 different
// scenarios:
//   - If the embedder supports opening Payment Handler window,
//   ContentBrowserClient::ShowPaymentHandlerWindow() returns true and tries to
//   open the window, then finally invokes
//   ShowPaymentHandlerWindowReplier::Run() to notify the result. In such a
//   case, the response callback |response_callback| of Mojo call
//   ServiceWorkerHost.OpenPaymentHandlerWindow() is bound into |callback| and
//   invoked there.
//   - Otherwise ContentBrowserClient::ShowPaymentHandlerWindow() just returns
//   false and does nothing else, then |this| will be dropped silently without
//   invoking Run(). In such a case, dtor of |this| invokes |fallback| (which
//   e.g. opens a normal window), |response_callback| is bound into |fallback|
//   and invoked there.
class ShowPaymentHandlerWindowReplier {
 public:
  ShowPaymentHandlerWindowReplier(
      PaymentHandlerSupport::ShowPaymentHandlerWindowCallback callback,
      PaymentHandlerSupport::OpenWindowFallback fallback,
      blink::mojom::ServiceWorkerHost::OpenPaymentHandlerWindowCallback
          response_callback)
      : callback_(std::move(callback)),
        fallback_(std::move(fallback)),
        response_callback_(std::move(response_callback)) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  ~ShowPaymentHandlerWindowReplier() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (response_callback_) {
      DCHECK(fallback_);
      base::PostTask(
          FROM_HERE, {ServiceWorkerContext::GetCoreThreadId()},
          base::BindOnce(std::move(fallback_), std::move(response_callback_)));
    }
  }

  void Run(bool success, int render_process_id, int render_frame_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RunOrPostTaskOnThread(
        FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
        base::BindOnce(std::move(callback_), std::move(response_callback_),
                       success, render_process_id, render_frame_id));
  }

 private:
  PaymentHandlerSupport::ShowPaymentHandlerWindowCallback callback_;
  PaymentHandlerSupport::OpenWindowFallback fallback_;
  blink::mojom::ServiceWorkerHost::OpenPaymentHandlerWindowCallback
      response_callback_;

  DISALLOW_COPY_AND_ASSIGN(ShowPaymentHandlerWindowReplier);
};

void ShowPaymentHandlerWindowOnUI(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    const GURL& url,
    PaymentHandlerSupport::ShowPaymentHandlerWindowCallback callback,
    PaymentHandlerSupport::OpenWindowFallback fallback,
    blink::mojom::ServiceWorkerHost::OpenPaymentHandlerWindowCallback
        response_callback) {
  GetContentClient()->browser()->ShowPaymentHandlerWindow(
      context_wrapper->storage_partition()->browser_context(), url,
      base::BindOnce(&ShowPaymentHandlerWindowReplier::Run,
                     std::make_unique<ShowPaymentHandlerWindowReplier>(
                         std::move(callback), std::move(fallback),
                         std::move(response_callback))));
}

}  // namespace

// static
void PaymentHandlerSupport::ShowPaymentHandlerWindow(
    const GURL& url,
    ServiceWorkerContextCore* context,
    ShowPaymentHandlerWindowCallback callback,
    OpenWindowFallback fallback,
    blink::mojom::ServiceWorkerHost::OpenPaymentHandlerWindowCallback
        response_callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(context);
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&ShowPaymentHandlerWindowOnUI,
                     base::WrapRefCounted(context->wrapper()), url,
                     std::move(callback), std::move(fallback),
                     std::move(response_callback)));
}

}  // namespace content
