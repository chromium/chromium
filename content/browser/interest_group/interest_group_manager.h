// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"

namespace content {

// InterestGroupManager acts as a proxy to access the InterestGroupStorage.
// Calls to the proxy functions can be called concurrently from multiple
// sequences as they are executed serially on a separate (blocking) sequence.
class CONTENT_EXPORT InterestGroupManager {
 public:
  // Creates an interest group manager using the provided directory path for
  // storage. If in_memory is true the path is ignored and an in-memory
  // database is used instead.
  explicit InterestGroupManager(const base::FilePath& path, bool in_memory);
  ~InterestGroupManager();
  InterestGroupManager(const InterestGroupManager& other) = delete;
  InterestGroupManager& operator=(const InterestGroupManager& other) = delete;

  /******** Proxy function calls to InterestGroupsStorageSqlImpl **********/

  // Joins an interest group. If the interest group does not exist, a new one
  // is created based on the provided group information. If the interest group
  // exists, the existing interest group is overwritten. In either case a join
  // record for this interest group is created.
  void JoinInterestGroup(blink::mojom::InterestGroupPtr group);
  // Remove the interest group if it exists.
  void LeaveInterestGroup(const url::Origin& owner, const std::string& name);
  // Updates the interest group of the same name based on the information in
  // the provided group. This does not update the interest group expiration
  // time or user bidding signals. Silently fails if the interest group does
  // not exist.
  void UpdateInterestGroup(blink::mojom::InterestGroupPtr group);
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
      base::OnceCallback<
          void(std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>)>
          callback);

  // Clear out storage for the matching owning origin. If the callback is empty
  // then apply to all origins.
  void DeleteInterestGroupData(
      base::RepeatingCallback<bool(const url::Origin&)> origin_matcher);

 private:
  // Remote for accessing the interest group storage from the browser UI thread.
  base::SequenceBound<InterestGroupStorage> impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_H_