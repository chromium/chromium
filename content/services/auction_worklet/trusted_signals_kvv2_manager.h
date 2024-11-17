// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_MANAGER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/cbor/values.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace auction_worklet {

class AuctionV8Helper;

// Class to manage and parse requests for trusted key-value server version 2
// requests from the browser-side TrustedSignalsCache. Each instance is scoped
// to an AuctionWorkletService, and may be used to fetch results that come from
// different trusted signals URLs, and a single instance may be used by multiple
// BidderWorklets/SellerWorklets.
//
// Callers request signals using a `compression_group_token` provided by the
// browser process. This class merges requests for the same compression group.
// Once the requested compression group has been provided by the browser process
// as a compressed CBOR string, this class decompresses and parses it. It then
// distributes the data to all consumers that requested it, and keeps the parsed
// data cached as long as there's any live consumer of the data, handing it out
// to satisfy new requests.
//
// All fields of all partitions in all requested compression groups are
// currently parsed up front, rather than on first use, so they can safely be
// reused on different V8 threads.
//
// TODO(crbug.com/365957549): Implement some way to not have to parse an entire
// compression group when only limited data is needed from it.
class CONTENT_EXPORT TrustedSignalsKVv2Manager
    : public mojom::TrustedSignalsCacheClient {
 public:
  using Result = TrustedSignals::Result;
  using SignalsType = TrustedSignalsKVv2ResponseParser::SignalsType;
  using PartitionMapOrError =
      TrustedSignalsKVv2ResponseParser::PartitionMapOrError;

  using ResultOrError = base::expected<scoped_refptr<Result>, std::string>;

  // Use common callback type to make it easy to use both this and a
  // TrustedSignalsRequestManager. Once TrustedSignalsRequestManager has been
  // removed, may make sense to switch to a callback that takes a ResultOrError
  // instead.
  using LoadSignalsCallback = TrustedSignalsRequestManager::LoadSignalsCallback;

  // Represents a single pending request for TrustedSignals from a consumer.
  // Destroying it cancels the request. All live Requests must be destroyed
  // before the TrustedSignalsKVv2Manager.
  class Request {
   public:
    Request(Request&) = delete;
    Request& operator=(Request&) = delete;
    virtual ~Request() = default;

   protected:
    Request() = default;
  };

  TrustedSignalsKVv2Manager(
      mojo::PendingRemote<mojom::TrustedSignalsCache> trusted_signals_cache,
      scoped_refptr<AuctionV8Helper> v8_helper);
  TrustedSignalsKVv2Manager(TrustedSignalsKVv2Manager&) = delete;

  ~TrustedSignalsKVv2Manager() override;

  TrustedSignalsKVv2Manager& operator=(TrustedSignalsKVv2Manager&) = delete;

  // Requests signals from the cache with `compression_group_token`, and parses
  // the `partition_id` partition as `signals_type` signals. The signals for the
  // specified partition will asynchronously be passed back to the passed in
  // callback.
  std::unique_ptr<Request> RequestSignals(
      SignalsType signals_type,
      base::UnguessableToken compression_group_token,
      int partition_id,
      LoadSignalsCallback load_signals_callback);

 private:
  // Private implementation of Request.
  class RequestImpl;

  // Tracks the data for a particular compression group, and all pending
  // Requests associated with it. Created when the first request with a new
  // UnguessableToken is received, at which point, the data is requested, and
  // destroyed when all Requests associated with it have been destroyed. Once
  // the data is received and has been parsed, it's still kept alive to
  // distribute the parsed data (or error) to new incoming requests for the same
  // compression group.
  struct CompressionGroup;

  // Map of the IDs identifying each compression group to each CompressionGroup.
  using CompressionGroupMap =
      std::map<base::UnguessableToken, CompressionGroup>;

  // A map of partition IDs to the result of parsing each partition.
  using PartitionMap = std::map<int, scoped_refptr<Result>>;

  // mojom::TrustedSignalsCacheClient implementation:
  void OnSuccess(mojom::TrustedSignalsCompressionScheme compression_scheme,
                 mojo_base::BigBuffer compression_group_data) override;
  void OnError(const std::string& error_message) override;

  // Called by OnComplete() once parsing has completed. Distributed the result
  // to all waiting requests, and stores it for future incoming requests.
  void OnComplete(base::UnguessableToken compression_group_token,
                  PartitionMapOrError parsed_compression_group_result);

  // Closes the Mojo pipe in `compression_group_pipes_` associated with the
  // specified CompressionGroup and clears the compression group's
  // mojo::ReceiverId. The passed in CompressionGroup must have a live pipe.
  void ClosePipe(CompressionGroupMap::iterator compression_group_it);

  // Called by RequestImpl when it's destroyed. Removes association between the
  // request and the CompressionGroup, destroying the group if it has no more
  // associated requests.
  void OnRequestDestroyed(RequestImpl* request,
                          CompressionGroupMap::iterator compression_group_it);

  // Retrieves the result for `partition_id` from `compression_group`. Even if
  // the compression group was fetched and parsed successfully, may return an
  // error if the partition is missing from the group.
  static ResultOrError GetResultForPartition(
      const CompressionGroup& compression_group,
      int partition_id);

  // Map of compression group IDs to CompressionGroups. A compression group is
  // destroyed when all Requests associated with it have been destroyed.
  CompressionGroupMap compression_groups_;

  mojo::Remote<mojom::TrustedSignalsCache> trusted_signals_cache_;

  // Set of receiver pipes associated with each CompressionGroup that's waiting
  // on data.
  mojo::ReceiverSet<mojom::TrustedSignalsCacheClient,
                    CompressionGroupMap::iterator>
      compression_group_pipes_;

  const scoped_refptr<AuctionV8Helper> v8_helper_;

  base::WeakPtrFactory<TrustedSignalsKVv2Manager> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_TRUSTED_SIGNALS_KVV2_MANAGER_H_
