// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8.h"

namespace auction_worklet {

class AuctionV8Helper;
class WorkletLoader;

// Represents a seller worklet for FLEDGE
// (https://github.com/WICG/turtledove/blob/main/FLEDGE.md). Loads and runs the
// seller worklet's Javascript.
class SellerWorklet : public mojom::SellerWorklet {
 public:
  // Starts loading the worklet script on construction. Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  // Must be destroyed before `v8_helper`.
  SellerWorklet(AuctionV8Helper* v8_helper,
                mojo::PendingRemote<network::mojom::URLLoaderFactory>
                    pending_url_loader_factory,
                const GURL& script_source_url,
                mojom::AuctionWorkletService::LoadSellerWorkletCallback
                    load_worklet_callback);

  explicit SellerWorklet(const SellerWorklet&) = delete;
  SellerWorklet& operator=(const SellerWorklet&) = delete;

  ~SellerWorklet() override;

  // mojom::SellerWorklet implementation:
  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               blink::mojom::AuctionAdConfigPtr auction_config,
               const url::Origin& browser_signal_top_window_origin,
               const url::Origin& browser_signal_interest_group_owner,
               const std::string& browser_signal_ad_render_fingerprint,
               uint32_t browser_signal_bidding_duration_msecs,
               ScoreAdCallback callback) override;
  void ReportResult(blink::mojom::AuctionAdConfigPtr auction_config,
                    const url::Origin& browser_signal_top_window_origin,
                    const url::Origin& browser_signal_interest_group_owner,
                    const GURL& browser_signal_render_url,
                    const std::string& browser_signal_ad_render_fingerprint,
                    double browser_signal_bid,
                    double browser_signal_desirability,
                    ReportResultCallback callback) override;

 private:
  void OnDownloadComplete(
      std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script,
      absl::optional<std::string> error_msg);

  AuctionV8Helper* const v8_helper_;

  const GURL script_source_url_;
  std::unique_ptr<WorkletLoader> worklet_loader_;

  mojom::AuctionWorkletService::LoadSellerWorkletCallback
      load_worklet_callback_;
  // Compiled script, not bound to any context. Can be repeatedly bound to
  // different context and executed, without persisting any state.
  std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
