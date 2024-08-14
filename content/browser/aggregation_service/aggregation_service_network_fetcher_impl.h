// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_NETWORK_FETCHER_IMPL_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_NETWORK_FETCHER_IMPL_H_

#include <list>
#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

class GURL;

namespace base {
class Clock;
class Time;
}  // namespace base

namespace content {

class StoragePartition;

// This class fetches a JSON formatted response from a helper server and uses a
// sandboxed utility process to parse it to a PublicKey array.
class CONTENT_EXPORT AggregationServiceNetworkFetcherImpl
    : public AggregationServiceKeyFetcher::NetworkFetcher {
 public:
  // `clock` must be a non-null pointer that is valid as long as this object.
  AggregationServiceNetworkFetcherImpl(const base::Clock* clock,
                                       StoragePartition* storage_partition);
  AggregationServiceNetworkFetcherImpl(
      const AggregationServiceNetworkFetcherImpl&) = delete;
  AggregationServiceNetworkFetcherImpl& operator=(
      const AggregationServiceNetworkFetcherImpl&) = delete;
  ~AggregationServiceNetworkFetcherImpl() override;

  void FetchPublicKeys(const GURL& url, NetworkFetchCallback callback) override;

  // Used by tests to inject a TestURLLoaderFactory so they can mock the
  // network response. Also used by the aggregation service tool to inject a
  // `url_loader_factory` if one is provided.
  static std::unique_ptr<AggregationServiceNetworkFetcherImpl> CreateForTesting(
      const base::Clock* clock,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool enable_debug_logging = false);

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FetchStatus {
    // The public key was fetched successfully.
    kSuccess = 0,

    // Failed to download the JSON file.
    kDownloadError = 1,

    // Failed to parse the JSON string.
    kJsonParseError = 2,

    // Invalid format or invalid keys were specified in the JSON string.
    kInvalidKeyError = 3,

    // The response has expired.
    kExpiredKeyError = 4,

    kMaxValue = kExpiredKeyError,
  };

  // This is a std::list so that iterators remain valid during modifications.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // For testing only.
  AggregationServiceNetworkFetcherImpl(
      const base::Clock* clock,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool enable_debug_logging);

  // Invoked from SimpleURLLoader after download is complete.
  void OnSimpleLoaderComplete(UrlLoaderList::iterator it,
                              const GURL& url,
                              NetworkFetchCallback callback,
                              std::unique_ptr<std::string> response_body);

  // Callback for DataDecoder. `expiry_time` will be null if the freshness
  // lifetime is zero.
  void OnJsonParse(const GURL& url,
                   NetworkFetchCallback callback,
                   base::Time fetch_time,
                   base::Time expiry_time,
                   data_decoder::DataDecoder::ValueOrError result);

  void OnError(const GURL& url,
               NetworkFetchCallback callback,
               FetchStatus status,
               std::string_view error_msg);

  void RecordFetchStatus(FetchStatus status) const;

  // Download requests that are in progress.
  UrlLoaderList loaders_in_progress_;

  const raw_ref<const base::Clock> clock_;

  // Might be `nullptr` for testing, otherwise must outlive `this`.
  raw_ptr<StoragePartition> storage_partition_;

  // Lazily accessed URLLoaderFactory used for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Whether to enable debug logging. Should be false in production.
  bool enable_debug_logging_ = false;

  base::WeakPtrFactory<AggregationServiceNetworkFetcherImpl> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_NETWORK_FETCHER_IMPL_H_
