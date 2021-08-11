// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace base {

class FilePath;

}  // namespace base

namespace network {

class SimpleURLLoader;
class SharedURLLoaderFactory;

}  // namespace network

namespace content {

// InterestGroupManager is a per-StoragePartition class that owns shared
// state needed to run FLEDGE auctions. It lives on the UI thread.
//
// It acts as a proxy to access an InterestGroupStorage, which lives off-thread
// as it performs blocking file IO when backed by on-disk storage.
class CONTENT_EXPORT InterestGroupManager {
 public:
  // Creates an interest group manager using the provided directory path for
  // persistent storage. If `in_memory` is true the path is ignored and only
  // in-memory storage is used.
  explicit InterestGroupManager(
      const base::FilePath& path,
      bool in_memory,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~InterestGroupManager();
  InterestGroupManager(const InterestGroupManager& other) = delete;
  InterestGroupManager& operator=(const InterestGroupManager& other) = delete;

  /******** Proxy function calls to InterestGroupsStorage **********/

  // Joins an interest group. If the interest group does not exist, a new one
  // is created based on the provided group information. If the interest group
  // exists, the existing interest group is overwritten. In either case a join
  // record for this interest group is created.
  void JoinInterestGroup(blink::InterestGroup group, const GURL& joining_url);
  // Remove the interest group if it exists.
  void LeaveInterestGroup(const url::Origin& owner, const std::string& name);
  // Updates the interest group of the same name based on the information in
  // the provided group. This does not update the interest group expiration
  // time or user bidding signals. Silently fails if the interest group does
  // not exist.
  void UpdateInterestGroup(blink::InterestGroup group);
  // Loads all interest groups owned by `owner`, then updates their definitions
  // by fetching their `dailyUpdateUrl`. Interest group updates that fail to
  // load or validate are skipped, but other updates will proceed.
  void UpdateInterestGroupsOfOwner(const url::Origin& owner);
  // Adds an entry to the bidding history for this interest group.
  void RecordInterestGroupBid(const url::Origin& owner,
                              const std::string& name);
  // Adds an entry to the win history for this interest group. `ad_json` is a
  // piece of opaque data to identify the winning ad.
  void RecordInterestGroupWin(const ::url::Origin& owner,
                              const std::string& name,
                              const std::string& ad_json);
  // Gets a list of all interest group owners. Each owner will only appear
  // once.
  void GetAllInterestGroupOwners(
      base::OnceCallback<void(std::vector<url::Origin>)> callback);
  // Gets a list of all interest groups with their bidding information
  // associated with the provided owner.
  void GetInterestGroupsForOwner(
      const url::Origin& owner,
      base::OnceCallback<void(std::vector<BiddingInterestGroup>)> callback);
  // Clear out storage for the matching owning origin. If the callback is empty
  // then apply to all origins.
  void DeleteInterestGroupData(
      base::RepeatingCallback<bool(const url::Origin&)> origin_matcher);
  // Get the last maintenance time from the underlying InterestGroupStorage.
  void GetLastMaintenanceTimeForTesting(
      base::RepeatingCallback<void(base::Time)> callback) const;

  AuctionProcessManager& auction_process_manager() {
    return *auction_process_manager_;
  }

  // Allows the AuctionProcessManager to be overridden in unit tests, both to
  // allow not creating a new process, and mocking out the Mojo service
  // interface.
  void set_auction_process_manager_for_testing(
      std::unique_ptr<AuctionProcessManager> auction_process_manager) {
    auction_process_manager_ = std::move(auction_process_manager);
  }

 private:
  void DidUpdateInterestGroupsOfOwnerDbLoad(
      url::Origin owner,
      std::vector<BiddingInterestGroup> interest_groups);
  void DidUpdateInterestGroupsOfOwnerNetFetch(
      network::SimpleURLLoader* simple_url_loader,
      url::Origin owner,
      std::string name,
      std::unique_ptr<std::string> fetch_body);
  void DidUpdateInterestGroupsOfOwnerJsonParse(
      url::Origin owner,
      std::string name,
      data_decoder::DataDecoder::ValueOrError result);

  // Owns and manages access to the InterestGroupStorage living on a different
  // thread.
  base::SequenceBound<InterestGroupStorage> impl_;

  // Stored as pointer so that tests can override it.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  // Used for fetching interest group update JSON over the network.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // All active network requests -- active requests will be cancelled when
  // destroyed.
  base::flat_set<std::unique_ptr<network::SimpleURLLoader>,
                 base::UniquePtrComparator>
      url_loaders_;

  // TODO(crbug.com/1186444): Do we need to test InterestGroupManager
  // destruction during update? If so, how?
  base::WeakPtrFactory<InterestGroupManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_H_
