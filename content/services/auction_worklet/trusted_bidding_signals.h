// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_BIDDING_SIGNALS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_BIDDING_SIGNALS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionDownloader;
class AuctionV8Helper;

// Represents the trusted bidding signals that are part of the FLEDGE bidding
// system (https://github.com/WICG/turtledove/blob/main/FLEDGE.md). Fetches and
// parses the hosted JSON data files needed by the bidder worklets.
//
// TODO(mmenke): This class currently does 4 copies when loading the data (To V8
// string, use V8's JSON parser, split data into V8 JSON subcomponent strings,
// convert to C++ strings), and 2 copies of each substring to use the data (To
// V8 per-key JSON string, use V8's JSON parser). Keeping the data stored as V8
// JSON subcomponents would remove 2 copies, without too much complexity. Could
// even implement V8 deep-copy logic, to remove two more copies (counting the
// clone operation as a copy).
class TrustedBiddingSignals {
 public:
  // Contains the values returned by the server.
  //
  // This can be created and destroyed on any thread, but GetSignals() can only
  // be used on the V8 thread.
  class Result {
   public:
    explicit Result(std::map<std::string, std::string> json_data);
    explicit Result(const Result&) = delete;
    ~Result();
    Result& operator=(const Result&) = delete;

    // Get the signals associated with the provided |keys|. `v8_helper`'s
    // Isolate must be active (in particular, this must be on the v8 thread),
    // and `context` must be the active context. |keys| must be a subset of
    // those provided when creating the TrustedBiddingSignals object. Always
    // returns a non-empty value (which may be an Object with no fields).
    v8::Local<v8::Object> GetSignals(
        AuctionV8Helper* v8_helper,
        v8::Local<v8::Context> context,
        const std::vector<std::string>& trusted_bidding_signals_keys) const;

   private:
    // Map of keys to their associated JSON data.
    std::map<std::string, std::string> json_data_;
  };

  using LoadSignalsCallback =
      base::OnceCallback<void(std::unique_ptr<Result> result,
                              absl::optional<std::string> error_msg)>;

  // Starts loading the JSON data on construction. `trusted_bidding_signals_url`
  // must be the base URL (no query params added). Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred.
  // Fails if the URL already has a query param (or has a location or embedded
  // credentials) or if the response is not JSON. If some or all keys are
  // missing, still succeeds, and GetSignals() will populate them with nulls.
  TrustedBiddingSignals(network::mojom::URLLoaderFactory* url_loader_factory,
                        std::vector<std::string> trusted_bidding_signals_keys,
                        const std::string& hostname,
                        const GURL& trusted_bidding_signals_url,
                        scoped_refptr<AuctionV8Helper> v8_helper,
                        LoadSignalsCallback load_signals_callback);
  explicit TrustedBiddingSignals(const TrustedBiddingSignals&) = delete;
  TrustedBiddingSignals& operator=(const TrustedBiddingSignals&) = delete;
  ~TrustedBiddingSignals();

 private:
  void OnDownloadComplete(std::vector<std::string> trusted_bidding_signals_keys,
                          std::unique_ptr<std::string> body,
                          absl::optional<std::string> error_msg);

  static void HandleDownloadResultOnV8Thread(
      scoped_refptr<AuctionV8Helper> v8_helper,
      const GURL& trusted_bidding_signals_url,
      std::vector<std::string> trusted_bidding_signals_keys,
      std::unique_ptr<std::string> body,
      absl::optional<std::string> error_msg,
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<TrustedBiddingSignals> weak_instance);

  // Called from V8 thread.
  static void PostCallbackToUserThread(
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<TrustedBiddingSignals> weak_instance,
      std::unique_ptr<Result> result,
      absl::optional<std::string> error_msg);

  // Called on user thread.
  void DeliverCallbackOnUserThread(std::unique_ptr<Result>,
                                   absl::optional<std::string> error_msg);

  const GURL trusted_bidding_signals_url_;  // original, for error messages.
  const scoped_refptr<AuctionV8Helper> v8_helper_;

  LoadSignalsCallback load_signals_callback_;
  std::unique_ptr<AuctionDownloader> auction_downloader_;

  base::WeakPtrFactory<TrustedBiddingSignals> weak_ptr_factory{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_BIDDING_SIGNALS_H_
