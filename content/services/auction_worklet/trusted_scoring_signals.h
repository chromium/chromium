// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SCORING_SIGNALS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SCORING_SIGNALS_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionDownloader;
class AuctionV8Helper;

// Represents the trusted scoring signals that are part of the FLEDGE bidding
// system (https://github.com/WICG/turtledove/blob/main/FLEDGE.md). Fetches and
// parses the hosted JSON data files needed by the seller worklets.
//
// TODO(mmenke): This class currently does 4 copies when loading the data (To V8
// string, use V8's JSON parser, split data into V8 JSON subcomponent strings,
// convert to C++ strings), and 2 copies of each substring to use the data (To
// V8 per-key JSON string, use V8's JSON parser). Keeping the data stored as V8
// JSON subcomponents would remove 2 copies, without too much complexity. Could
// even implement V8 deep-copy logic, to remove two more copies (counting the
// clone operation as a copy).
class TrustedScoringSignals {
 public:
  // Contains the values returned by the server.
  //
  // This can be created and destroyed on any thread, but GetSignals() can only
  // be used on the V8 thread.
  class Result {
   public:
    Result(std::map<GURL, std::string> render_url_json_data,
           std::map<GURL, std::string> ad_component_json_data);
    explicit Result(const Result&) = delete;
    ~Result();
    Result& operator=(const Result&) = delete;

    // Retrieves the trusted scoring signals associated with the passed in urls,
    // in the format expected by a worklet's scoreAd() method. `v8_helper`'s
    // Isolate must be active (in particular, this must be on the v8 thread),
    // and `context` must be the active context. `render_url` and
    // `ad_component_render_urls` must be subsets of the corresponding sets of
    // GURLs provided when creating the TrustedScoringSignals object. Always
    // returns a non-empty value.
    v8::Local<v8::Object> GetSignals(
        AuctionV8Helper* v8_helper,
        v8::Local<v8::Context> context,
        const GURL& render_url,
        const std::set<GURL>& ad_component_render_urls) const;

   private:
    // Map of GURLs to their associated JSON data.
    std::map<GURL, std::string> render_url_json_data_;
    std::map<GURL, std::string> ad_component_json_data_;
  };

  using LoadSignalsCallback =
      base::OnceCallback<void(std::unique_ptr<Result> result,
                              absl::optional<std::string> error_msg)>;

  // Starts loading the JSON data on construction. `trusted_scoring_signals_url`
  // must be the base URL (no query params added). Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  // Fails if the URL already has a query param (or has a location or embedded
  // credentials) or if the response is not JSON. If some or all of the render
  // URLs are missing, still succeeds, and GetSignals() will populate them with
  // nulls.
  //
  // There are no lifetime constraints of `url_loader_factory`.
  TrustedScoringSignals(network::mojom::URLLoaderFactory* url_loader_factory,
                        std::set<GURL> render_urls,
                        std::set<GURL> ad_component_render_urls,
                        const std::string& hostname,
                        const GURL& trusted_scoring_signals_url,
                        scoped_refptr<AuctionV8Helper> v8_helper,
                        LoadSignalsCallback load_signals_callback);
  explicit TrustedScoringSignals(const TrustedScoringSignals&) = delete;
  TrustedScoringSignals& operator=(const TrustedScoringSignals&) = delete;
  ~TrustedScoringSignals();

 private:
  void OnDownloadComplete(std::set<GURL> render_urls,
                          std::set<GURL> ad_component_render_urls,
                          std::unique_ptr<std::string> body,
                          absl::optional<std::string> error_msg);

  static void HandleDownloadResultOnV8Thread(
      scoped_refptr<AuctionV8Helper> v8_helper,
      const GURL& trusted_scoring_signals_url,
      std::set<GURL> render_urls,
      std::set<GURL> ad_component_render_urls,
      std::unique_ptr<std::string> body,
      absl::optional<std::string> error_msg,
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<TrustedScoringSignals> weak_instance);

  // Called from V8 thread.
  static void PostCallbackToUserThread(
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<TrustedScoringSignals> weak_instance,
      std::unique_ptr<Result> result,
      absl::optional<std::string> error_msg);

  // Called on user thread.
  void DeliverCallbackOnUserThread(std::unique_ptr<Result>,
                                   absl::optional<std::string> error_msg);

  const GURL trusted_scoring_signals_url_;  // original, for error messages.
  const scoped_refptr<AuctionV8Helper> v8_helper_;

  LoadSignalsCallback load_signals_callback_;
  std::unique_ptr<AuctionDownloader> auction_downloader_;

  base::WeakPtrFactory<TrustedScoringSignals> weak_ptr_factory{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SCORING_SIGNALS_H_
