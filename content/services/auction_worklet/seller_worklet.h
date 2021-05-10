// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
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
class SellerWorklet {
 public:
  // The result of invoking reportResult().
  struct Report {
    // Creates a Report for a failure.
    Report();

    // Creates a Report for a successful call.
    Report(std::string signals_for_winner, GURL report_url);

    // Creates a ScoreResult representing a fatal error, potentially with a
    // helpful diagnostic message in `error_msg`.
    explicit Report(base::Optional<std::string> error_msg);

    Report(const Report& other);
    Report(Report&& other);

    ~Report();

    Report& operator=(const Report&);
    Report& operator=(Report&&);

    // `success` will be false on any type of failure, including lack of a
    // method.
    bool success = false;

    // JSON data as a string. Sent to the winner's ReportWin function. JSON
    // instead of a v8 Value for sanitization.
    std::string signals_for_winner;

    // Report URL, if one is provided. Empty on failure, or if no report URL is
    // provided.
    GURL report_url;

    // Error message for debugging. This isn't guaranteed to have a value for
    // all failures.
    base::Optional<std::string> error_msg;
  };

  using LoadWorkletCallback =
      base::OnceCallback<void(bool success,
                              base::Optional<std::string> error_msg)>;

  // Callback for ScoreAd(). On success, `score` is the positive score returned
  // by the script. On failure, it's 0. `errors` is a vector of any errors that
  // occurred while running the script.
  using ScoreAdCallback =
      base::OnceCallback<void(double score,
                              const std::vector<std::string>& errors)>;

  // Starts loading the worklet script on construction. Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  // Must be destroyed before `v8_helper`.
  SellerWorklet(network::mojom::URLLoaderFactory* url_loader_factory,
                const GURL& script_source_url,
                AuctionV8Helper* v8_helper,
                LoadWorkletCallback load_worklet_callback);
  explicit SellerWorklet(const SellerWorklet&) = delete;
  SellerWorklet& operator=(const SellerWorklet&) = delete;
  ~SellerWorklet();

  // Calls scoreAd(), and invokes passed in callback asynchronously with the
  // resulting score. May only be called once the worklet has successfully
  // completed loaded. No data is leaked between consecutive invocations of this
  // method, or between invocations of this method and ReportResult().
  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               const blink::mojom::AuctionAdConfig& auction_config,
               const std::string& browser_signal_top_window_hostname,
               const url::Origin& browser_signal_interest_group_owner,
               const std::string& browser_signal_ad_render_fingerprint,
               base::TimeDelta browser_signal_bidding_duration,
               ScoreAdCallback callback);

  // Calls reportResult(), and invokes passed in callback asynchronously with
  // the reporting information. May only be called once the worklet has
  // successfully loaded.
  void ReportResult(const blink::mojom::AuctionAdConfig& auction_config,
                    const std::string& browser_signal_top_window_hostname,
                    const url::Origin& browser_signal_interest_group_owner,
                    const GURL& browser_signal_render_url,
                    const std::string& browser_signal_ad_render_fingerprint,
                    double browser_signal_bid,
                    double browser_signal_desirability,
                    base::OnceCallback<void(Report)> callback);

 private:
  void OnDownloadComplete(
      LoadWorkletCallback load_worklet_callback,
      std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script,
      base::Optional<std::string> error_msg);

  const GURL script_source_url_;
  AuctionV8Helper* const v8_helper_;
  std::unique_ptr<WorkletLoader> worklet_loader_;

  // Compiled script, not bound to any context. Can be repeatedly bound to
  // different context and executed, without persisting any state.
  std::unique_ptr<v8::Global<v8::UnboundScript>> worklet_script_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SELLER_WORKLET_H_
