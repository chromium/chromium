// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace content {

class PaymentInstrumentIconFetcher {
 public:
  using PaymentInstrumentIconFetcherCallback =
      base::OnceCallback<void(const std::string&)>;

  // Should be called on the service worker core thread.
  static void Start(
      const GURL& scope,
      std::unique_ptr<std::vector<GlobalFrameRoutingId>> provider_hosts,
      const std::vector<blink::Manifest::ImageResource>& icons,
      PaymentInstrumentIconFetcherCallback callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PaymentInstrumentIconFetcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_
