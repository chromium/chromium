// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace content {

class PaymentInstrumentIconFetcher {
 public:
  using PaymentInstrumentIconFetcherCallback =
      base::OnceCallback<void(const std::string&)>;

  PaymentInstrumentIconFetcher() = delete;
  PaymentInstrumentIconFetcher(const PaymentInstrumentIconFetcher&) = delete;
  PaymentInstrumentIconFetcher& operator=(const PaymentInstrumentIconFetcher&) =
      delete;

  // Should be called on the UI thread.
  static void Start(
      const GURL& scope,
      std::unique_ptr<std::vector<GlobalRenderFrameHostId>> frame_routing_ids,
      const std::vector<blink::Manifest::ImageResource>& icons,
      PaymentInstrumentIconFetcherCallback callback);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_INSTRUMENT_ICON_FETCHER_H_
