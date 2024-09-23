// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/stored_payment_app.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

// Lives on the UI thread.
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

  PaymentAppInfoFetcher() = delete;
  PaymentAppInfoFetcher(const PaymentAppInfoFetcher&) = delete;
  PaymentAppInfoFetcher& operator=(const PaymentAppInfoFetcher&) = delete;

  static void Start(
      const GURL& context_url,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      PaymentAppInfoFetchCallback callback);

 private:
  class SelfDeleteFetcher {
   public:
    explicit SelfDeleteFetcher(PaymentAppInfoFetchCallback callback);

    SelfDeleteFetcher(const SelfDeleteFetcher&) = delete;
    SelfDeleteFetcher& operator=(const SelfDeleteFetcher&) = delete;

    ~SelfDeleteFetcher();

    void Start(const GURL& context_url,
               const std::unique_ptr<std::vector<GlobalRenderFrameHostId>>&
                   frame_routing_ids);

   private:
    void RunCallbackAndDestroy();

    // The WebContents::GetManifestCallback.
    void FetchPaymentAppManifestCallback(
        blink::mojom::ManifestRequestResult result,
        const GURL& url,
        blink::mojom::ManifestPtr manifest);

    // The ManifestIconDownloader::IconFetchCallback.
    void OnIconFetched(const SkBitmap& icon);

    // Prints the warning |message| in the DevTools console, if possible.
    // Otherwise logs the warning on command line.
    void WarnIfPossible(const std::string& message);

    GURL manifest_url_;
    GURL icon_url_;
    base::WeakPtr<WebContents> web_contents_;
    std::unique_ptr<PaymentAppInfo> fetched_payment_app_info_;
    PaymentAppInfoFetchCallback callback_;
    base::WeakPtrFactory<SelfDeleteFetcher> weak_ptr_factory_{this};
  };
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_INFO_FETCHER_H_
