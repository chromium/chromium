// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_K_ANONYMITY_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_K_ANONYMITY_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace content {
class InterestGroupManagerImpl;

// Calculates the k-anonymity key for an interest group from the owner and name
std::string KAnonKeyFor(const url::Origin& owner, const std::string& name);

// Manages k-anonymity updates. Checks last updated times in the database
// to limit updates (joins and queries) to once per day. Called by the
// InterestGroupManagerImpl for interest group k-anonymity updates. Calls
// The InterestGroupManagerImpl to access interest group storage to perform
// interest group updates.
class InterestGroupKAnonymityManager {
 public:
  InterestGroupKAnonymityManager(
      InterestGroupManagerImpl* interest_group_manager,
      std::unique_ptr<KAnonymityServiceDelegate> k_anonymity_service);
  ~InterestGroupKAnonymityManager();

  // Requests the k-anonymity status of elements of the interest group that
  // haven't been updated in 24 hours or more. Results are passed to
  // interest_group_manater_->UpdateKAnonymity.
  void QueryKAnonymityForInterestGroup(
      const StorageInterestGroup& storage_group);

  // Notify the k-anonymity service that we are joining this interest group.
  // Internally this calls RegisterIDAsJoined() for interest group name and
  // update URL.
  void RegisterInterestGroupAsJoined(const blink::InterestGroup& group);

  // Notify the k-anonymity service that this ad won an auction. Internally this
  // calls RegisterIDAsJoined().
  void RegisterAdAsWon(const GURL& render_url);

 private:
  // Callback from k-anonymity service QuerySets(). Saves the updated results to
  // the database by calling interest_group_manager_->UpdateKAnonymity for each
  // URL in query with the corresponding k-anonymity status from status.
  void QuerySetsCallback(std::vector<std::string> query,
                         base::Time update_time,
                         std::vector<bool> status);

  // Starts fetching the LastKAnonymityReported time for `url` from the
  // database.
  void RegisterIDAsJoined(const std::string& key);

  // Called by the database when the update time for `url` has been retrieved.
  // If the last reported time is too long ago, calls JoinSet() on the
  // k-anonymity service.
  void OnGotLastReportedTime(std::string key,
                             absl::optional<base::Time> last_update_time);

  // Callback from k-anonymity service JoinSet(). Updates the LastReported time
  // for key in the database, regardless of status (fail close).
  void JoinSetCallback(std::string key, bool status);

  // An unowned pointer to the InterestGroupManagerImpl that owns this
  // InterestGroupUpdateManager. Used as an intermediary to talk to the
  // database.
  raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

  std::unique_ptr<KAnonymityServiceDelegate> k_anonymity_service_;
  base::WeakPtrFactory<InterestGroupKAnonymityManager> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_K_ANONYMITY_MANAGER_H_
