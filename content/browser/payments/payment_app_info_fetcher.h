// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class PaymentAppInfoFetcher {
 public:
  struct PaymentAppInfo {
    PaymentAppInfo();
    ~PaymentAppInfo();

    std::string name;
    std::string icon;
    bool prefer_related_applications = false;
    std::vector<StoredRelatedApplication> related_applications;
  };
  using PaymentAppInfoFetchCallback =
      base::OnceCallback<void(std::unique_ptr<PaymentAppInfo> app_info)>;

  // Only accessed on the service worker core thread.
  static void Start(
      const GURL& context_url,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      PaymentAppInfoFetchCallback callback);

 private:
  // Only accessed on the UI thread.
  static void StartOnUI(
      const GURL& context_url,
      const std::unique_ptr<std::vector<GlobalFrameRoutingId>>& provider_hosts,
      PaymentAppInfoFetchCallback callback);

  // Keeps track of the web contents.
  // Only accessed on the UI thread.
  class WebContentsHelper : public WebContentsObserver {
   public:
    explicit WebContentsHelper(WebContents* web_contents);
    ~WebContentsHelper() override;
  };

  // Only accessed on the UI thread.
  class SelfDeleteFetcher {
   public:
    explicit SelfDeleteFetcher(PaymentAppInfoFetchCallback callback);
    ~SelfDeleteFetcher();

    void Start(const GURL& context_url,
               const std::unique_ptr<std::vector<GlobalFrameRoutingId>>&
                   provider_hosts);

   private:
    void RunCallbackAndDestroy();

    // The WebContents::GetManifestCallback.
    void FetchPaymentAppManifestCallback(const GURL& url,
                                         const blink::Manifest& manifest);

    // The ManifestIconDownloader::IconFetchCallback.
    void OnIconFetched(const SkBitmap& icon);

    // Prints the warning |message| in the DevTools console, if possible.
    // Otherwise logs the warning on command line.
    void WarnIfPossible(const std::string& message);

    GURL manifest_url_;
    GURL icon_url_;
    std::unique_ptr<WebContentsHelper> web_contents_helper_;
    std::unique_ptr<PaymentAppInfo> fetched_payment_app_info_;
    PaymentAppInfoFetchCallback callback_;
    base::WeakPtrFactory<SelfDeleteFetcher> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(SelfDeleteFetcher);
  };

  DISALLOW_IMPLICIT_CONSTRUCTORS(PaymentAppInfoFetcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_
