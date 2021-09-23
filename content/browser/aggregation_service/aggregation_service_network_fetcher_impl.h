// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_NETWORK_FETCHER_IMPL_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_NETWORK_FETCHER_IMPL_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace base {
class Clock;
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class StoragePartition;

// This class fetches a JSON formatted response from a helper server and uses a
// sandboxed utility process to parse it to a PublicKey array.
class CONTENT_EXPORT AggregationServiceNetworkFetcherImpl
    : public AggregationServiceKeyFetcher::NetworkFetcher {
 public:
  // `clock` and `storage_partition` must be non-null pointers that are valid as
  // long as this object.
  AggregationServiceNetworkFetcherImpl(const base::Clock* clock,
                                       StoragePartition* storage_partition);
  AggregationServiceNetworkFetcherImpl(
      const AggregationServiceNetworkFetcherImpl&) = delete;
  AggregationServiceNetworkFetcherImpl& operator=(
      const AggregationServiceNetworkFetcherImpl&) = delete;
  ~AggregationServiceNetworkFetcherImpl() override;

  void FetchPublicKeys(const url::Origin& origin,
                       NetworkFetchCallback callback) override;

  // Tests inject a TestURLLoaderFactory so they can mock the network response.
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  enum class FetchError {
    kDownload = 0,
    kJsonParse = 1,
    kMaxValue = kJsonParse,
  };

  // This is a std::list so that iterators remain valid during modifications.
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // Invoked from SimpleURLLoader after download is complete.
  void OnSimpleLoaderComplete(UrlLoaderList::iterator it,
                              const url::Origin& origin,
                              NetworkFetchCallback callback,
                              std::unique_ptr<std::string> response_body);

  // Callback for DataDecoder. `expiry_time` will be null if the freshness
  // lifetime is zero.
  void OnJsonParse(const url::Origin& origin,
                   NetworkFetchCallback callback,
                   base::Time fetch_time,
                   base::Time expiry_time,
                   data_decoder::DataDecoder::ValueOrError result);

  void OnError(const url::Origin& origin,
               NetworkFetchCallback callback,
               FetchError error,
               const std::string& error_msg);

  // Download requests that are in progress.
  UrlLoaderList loaders_in_progress_;

  const base::Clock& clock_;

  StoragePartition& storage_partition_;

  // Lazily accessed URLLoaderFactory used for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<AggregationServiceNetworkFetcherImpl> weak_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_NETWORK_FETCHER_IMPL_H_
