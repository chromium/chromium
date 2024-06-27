// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TRUSTED_SIGNALS_FETCHER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TRUSTED_SIGNALS_FETCHER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Single-use network fetcher for versions 2+ of the key-value server API.
// It takes a list compression groups and partitions, and asynchronously returns
// a set of responses, one per compression group. The responses are provided as
// still compressed compression group bodies, so the cache layer can store
// compressed responses and to minimize IPC size. The responses will be
// decompressed before use in the appropriate Javascript process.
//
// Bidding and scoring signals need different structs when sending requests, but
// they use the same response format, since it's only the compressed data itself
// that varies based on signals type.
//
// TODO(https://crbug.com/333445540): This is currently only an API, with no
// implementation. Need to actually implement the API.
class CONTENT_EXPORT TrustedSignalsFetcher {
 public:
  // All the data needed to request a particular bidding signals partition.
  //
  // TODO(https://crbug.com/333445540): Consider making some of these fields
  // pointers to reduce copies. Since tests use this class to store arguments,
  // would need to rework that as well.
  struct CONTENT_EXPORT BiddingPartition {
    BiddingPartition();
    BiddingPartition(BiddingPartition&&);

    ~BiddingPartition();

    BiddingPartition& operator=(BiddingPartition&&);

    int partition_id;

    std::set<std::string> interest_group_names;
    std::set<std::string> keys;
    std::string hostname;

    // At the moment, valid keys are "experimentGroupId", "slotSize", and
    // "allSlotsRequestedSizes". We could take them separately, but seems better
    // to take one field rather than several?
    base::Value::Dict additional_params;
  };

  // While buying and scoring signals partitions need different structs when
  // sending requests, the responses use the same format.

  // The received result for a particular compression group. Only returned on
  // success.
  struct CONTENT_EXPORT CompressionGroupResult {
    CompressionGroupResult();
    CompressionGroupResult(CompressionGroupResult&&);

    ~CompressionGroupResult();

    CompressionGroupResult& operator=(CompressionGroupResult&&);

    // The compression scheme used by `compression_group_data`, as indicated by
    // the server.
    auction_worklet::mojom::TrustedSignalsCompressionScheme compression_scheme;

    // The still-compressed data for the compression group.
    std::vector<uint8_t> compression_group_data;

    // Time until the response expires.
    base::TimeDelta ttl;
  };

  // A map of compression group ids to results, in the case of success.
  using CompressionGroupResultMap = std::map<int, CompressionGroupResult>;

  // The result type in the case of an error. Errors don't have a TTL.
  struct CONTENT_EXPORT ErrorInfo {
    std::string error_msg;
  };

  // The result of a fetch. Either the entire fetch succeeds or it fails with a
  // single error.
  using SignalsFetchResult =
      base::expected<CompressionGroupResultMap, ErrorInfo>;

  using Callback = base::OnceCallback<void(SignalsFetchResult)>;

  explicit TrustedSignalsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Virtual for tests.
  virtual ~TrustedSignalsFetcher();

  TrustedSignalsFetcher(const TrustedSignalsFetcher&) = delete;
  TrustedSignalsFetcher& operator=(const TrustedSignalsFetcher&) = delete;

  // `partitions` is a map of all partitions in the request, indexed by
  // compression group id. Virtual for tests.
  virtual void FetchBiddingSignals(
      const GURL& trusted_bidding_signals_url,
      const std::map<int, std::vector<BiddingPartition>>& compression_groups,
      Callback callback);

 private:
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TRUSTED_SIGNALS_FETCHER_H_
