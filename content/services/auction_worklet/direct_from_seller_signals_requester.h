// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_DIRECT_FROM_SELLER_SIGNALS_REQUESTER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_DIRECT_FROM_SELLER_SIGNALS_REQUESTER_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace net {

class HttpResponseHeaders;

}  // namespace net

namespace auction_worklet {

class AuctionDownloader;
class AuctionV8Helper;

// Requests DirectFromSellerSignals from subresource bundles, caching a *single*
// recently-fetched value, such as one of per-buyer, seller, or auction signals.
//
// If multiple incoming requests are made at the same time for the same uncached
// URL, the requests will be coalesced so that only one request is made.
class CONTENT_EXPORT DirectFromSellerSignalsRequester {
 public:
  // Contains the signals loaded from the subresource bundle, a null value and
  // error if fetching or parsing signals failed, or the default-constructed
  // value of null signals with no error.
  //
  // This can be created and destroyed on any sequence, but GetSignals() can
  // only be used on the V8 sequence.
  class CONTENT_EXPORT Result {
   public:
    // Constructs a Result with a null value with no error.
    Result();

    Result(Result&&);
    Result& operator=(Result&&);

    ~Result();

    // Parses the internal JSON string into a v8::Value.
    //
    // If there was an error requesting / parsing the signals, an error string
    // will be appended to `errors`, and a V8 null value will be returned. If
    // the Result was default-constructed, a V8 null value will be returned
    // without updating `errors`.
    v8::Local<v8::Value> GetSignals(AuctionV8Helper& v8_helper,
                                    v8::Local<v8::Context> context,
                                    std::vector<std::string>& errors) const;

    // Returns true if this Result is a null value, and false otherwise. Returns
    // false if Result is an error.
    bool IsNull() const;

   private:
    // Private methods are called by DirectFromSellerSignalsRequester.
    friend DirectFromSellerSignalsRequester;

    // Response JSON strings use thread-safe refcounting to allow multiple
    // Result objects to share the same response without copies.
    class ResponseString : public base::RefCountedThreadSafe<ResponseString> {
     public:
      explicit ResponseString(std::string&& other);

      explicit ResponseString(const ResponseString&) = delete;
      ResponseString& operator=(const ResponseString&) = delete;

      const std::string& value() const { return value_; }

     private:
      friend class base::RefCountedThreadSafe<ResponseString>;
      ~ResponseString();

      const std::string value_;
    };

    // Error strings are small, and are stored directly.
    using ErrorString = base::StrongAlias<class ErrorStringTag, std::string>;

    // A network request always results in a response, or an error.
    //
    // Default-constructed, this will be a null scoped_refptr<ResponseString>.
    using ResponseOrError =
        absl::variant<scoped_refptr<ResponseString>, ErrorString>;

    // Constructs a Result based on the result of the network download.
    Result(GURL signals_url,
           std::unique_ptr<std::string> response_body,
           scoped_refptr<net::HttpResponseHeaders> headers,
           std::optional<std::string> error);

    // The copy constructor is used for internal caching, and for passing
    // results to every pending caller when a coalesced download completes.
    Result(const Result&);
    Result& operator=(const Result&);

    const GURL& signals_url() const { return signals_url_; }

    GURL signals_url_;
    ResponseOrError response_or_error_;
  };

  using DirectFromSellerSignalsRequesterCallback =
      base::OnceCallback<void(Result)>;

  // Represents a single pending request for DirectFromSellerSignals from a
  // consumer. Destroying it cancels the request. All live Requests must be
  // destroyed before the DirectFromSellerSignalsRequester used to create them.
  //
  // It is illegal to destroy other pending Requests when a Request's callback
  // is invoked.
  //
  // Requests must be destroyed on the sequence that called
  // DirectFromSellerSignalsRequester::LoadSignals().
  class CONTENT_EXPORT Request {
   public:
    ~Request();

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

   private:
    friend DirectFromSellerSignalsRequester;

    explicit Request(DirectFromSellerSignalsRequesterCallback callback,
                     DirectFromSellerSignalsRequester& requester,
                     const GURL& signals_url);

    // Methods to run the callback synchronously and asynchronously (by posting
    // to the SequencedTaskRunner::CurrentDefaultHandle).
    //
    // The async version uses WeakPtr, so it will be cancelled if this Request
    // object is destroyed. The sync version should only be used after a
    // download has completed, as these are subject to cancellation if the
    // corresponding AuctionDownloader (owned by this class) is destroyed.
    //
    // Note that the callback may destroy this Request.
    void RunCallbackSync(Result result);
    void RunCallbackAsync(Result result);

    void set_coalesce_iterator(std::list<raw_ptr<Request>>::iterator it) {
      DCHECK_EQ(*it, this);
      maybe_coalesce_iterator_ = it;
    }

    DirectFromSellerSignalsRequesterCallback callback_;

    // Never null.
    raw_ptr<DirectFromSellerSignalsRequester> requester_;

    // For looking up the list used with `maybe_coalesce_iterator_`.
    GURL signals_url_;

    // The iterator to the list of coalesced requests in `coalesced_downloads_`,
    // used for cancellation of callbacks and downloads.
    //
    // NOTE: This can be nullopt if serving from cache, or if the download
    // already completed -- it will have a value when there is still an
    // outstanding request for `signals_url_`.
    std::optional<std::list<raw_ptr<Request>>::iterator>
        maybe_coalesce_iterator_;

    // Must appear after all other members.
    base::WeakPtrFactory<Request> weak_factory_{this};
  };

  DirectFromSellerSignalsRequester();
  ~DirectFromSellerSignalsRequester();

  DirectFromSellerSignalsRequester(const DirectFromSellerSignalsRequester&) =
      delete;
  DirectFromSellerSignalsRequester& operator=(
      const DirectFromSellerSignalsRequester&) = delete;

  // Loads the signals from `signals_url`, providing the results to `callback`.
  //
  // `callback` will always be invoked, and invoked asynchronously, unless the
  // Request is deleted first, cancelling the request.
  std::unique_ptr<Request> LoadSignals(
      network::mojom::URLLoaderFactory& url_loader_factory,
      const GURL& signals_url,
      DirectFromSellerSignalsRequesterCallback callback);

 private:
  // Stores a single active download, along with all callbacks waiting on that
  // download.
  struct CoalescedDownload {
    explicit CoalescedDownload(std::unique_ptr<AuctionDownloader> downloader);
    ~CoalescedDownload();

    CoalescedDownload(const CoalescedDownload&) = delete;
    CoalescedDownload& operator=(const CoalescedDownload&) = delete;
    CoalescedDownload(CoalescedDownload&&);
    CoalescedDownload& operator=(CoalescedDownload&&);

    // The downloader downloading the current `coalesced_downloads_` GURL.
    std::unique_ptr<AuctionDownloader> downloader;

    // A list of all requests whose callbacks should be called when the download
    // completes.
    //
    // Result instances should remove themselves from this list upon their
    // destruction, and if the list is empty, remove the `cached_result_` pair
    // for their `signals_url_`.
    //
    // This guarantees that none of these raw pointers ever point to destroyed
    // Requests.
    std::list<raw_ptr<Request>> requests;
  };

  // Called only when the AuctionDownloader loads new signals.
  //
  // Validates headers, caches the results, and calls all callbacks held in
  // Result objects in `coalesced_downloads_` that are waiting on the URL.
  void OnSignalsDownloaded(GURL signals_url,
                           base::TimeTicks start_time,
                           std::unique_ptr<std::string> response_body,
                           scoped_refptr<net::HttpResponseHeaders> headers,
                           std::optional<std::string> error);

  void OnRequestDestroyed(Request& request);

  // The most recently-downloaded result is cached, along with its URL.
  // Initially, nothing is cached, and the URL is null.
  Result cached_result_;

  // For each URL that is actively downloading, stores its downloader, and the
  // list of all callbacks to be called when the download completes.
  std::map<GURL, CoalescedDownload> coalesced_downloads_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_DIRECT_FROM_SELLER_SIGNALS_REQUESTER_H_
