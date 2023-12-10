// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_K_ANONYMITY_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_K_ANONYMITY_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/common/content_export.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace content {
class InterestGroupManagerImpl;

// Maximum number of IDs to send in a single query call. Public for testing.
constexpr size_t kQueryBatchSizeLimit = 1000;

// Returns true if the k-anonymity data indicates that the k-anonymity key
// `data.key` should be considered k-anonymous at time `now`. To be k-anonymous
// `data.is_k_anonymous` must be true and `data.last_updated` must be less than
// 7 days ago.
bool CONTENT_EXPORT
IsKAnonymous(const StorageInterestGroup::KAnonymityData& data,
             const base::Time now);

// Manages k-anonymity updates. Checks last updated times in the database
// to limit updates (joins and queries) to once per day. Called by the
// InterestGroupManagerImpl for interest group k-anonymity updates. Calls
// The InterestGroupManagerImpl to access interest group storage to perform
// interest group updates.
class CONTENT_EXPORT InterestGroupKAnonymityManager {
 public:
  using GetKAnonymityServiceDelegateCallback =
      base::RepeatingCallback<KAnonymityServiceDelegate*()>;

  InterestGroupKAnonymityManager(
      InterestGroupManagerImpl* interest_group_manager,
      GetKAnonymityServiceDelegateCallback k_anonymity_service_callback);
  ~InterestGroupKAnonymityManager();

  // Requests the k-anonymity status of elements of the interest group that
  // haven't been updated in 24 hours or more (querying the database first to
  // get the applicable k-anon keys). Results are passed to
  // interest_group_manager_->UpdateKAnonymity.
  void QueryKAnonymityForInterestGroup(
      const blink::InterestGroupKey& interest_group_key);

  // Notify the k-anonymity service that these ad keys won an auction.
  // Internally this calls RegisterIDAsJoined().
  void RegisterAdKeysAsJoined(base::flat_set<std::string> keys);

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

  // Requests the k-anonymity status of elements of `k_anon_data` that
  // haven't been updated in 24 hours or more. Results are passed to
  // interest_group_manager_->UpdateKAnonymity.
  void QueryKAnonymityData(
      const std::vector<StorageInterestGroup::KAnonymityData>& k_anon_data);

  // An unowned pointer to the InterestGroupManagerImpl that owns this
  // InterestGroupUpdateManager. Used as an intermediary to talk to the
  // database.
  raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

  GetKAnonymityServiceDelegateCallback k_anonymity_service_callback_;

  // We keep track of joins in progress because the joins that haven't completed
  // are still marked as eligible but it would be incorrect to join them
  // multiple times. We don't do this for query because the size of the request
  // could expose membership in overlapping groups through traffic analysis.
  base::flat_set<std::string> joins_in_progress;

  base::WeakPtrFactory<InterestGroupKAnonymityManager> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_K_ANONYMITY_MANAGER_H_
