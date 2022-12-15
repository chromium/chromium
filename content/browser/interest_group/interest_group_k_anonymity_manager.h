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

class GURL;

namespace content {
class InterestGroupManagerImpl;

// Calculates the k-anonymity key for an interest group from the owner and name
std::string CONTENT_EXPORT KAnonKeyFor(const url::Origin& owner,
                                       const std::string& name);

// Calculates the k-anonymity key for an Ad that is used for determining if an
// ad is k-anonymous for the purposes of bidding and winning an auction.
// We want to avoid providing too much identifying information for event level
// reporting in reportWin. This key is used to check that providing the interest
// group owner and ad URL to the bidding script doesn't identify the user. It is
// used to gate whether an ad can participate in a FLEDGE auction because event
// level reports need to include both the owner and ad URL for the purposes of
// an auction.
// TODO(behamilton): Use a different key for ad components.
std::string CONTENT_EXPORT KAnonKeyForAdBid(const blink::InterestGroup& group,
                                            const GURL& ad_url);

// Given a key computed by KAnonKeyForAdBid, returns the `render_url` of the
// ad that was used to produce it.
GURL CONTENT_EXPORT RenderUrlFromKAnonKeyForAdBid(const std::string& key);

// Calculates the k-anonymity key for reporting the interest group name in
// reportWin along with the given Ad.
// We want to avoid providing too much identifying information for event level
// reporting in reportWin. This key is used to check if including the interest
// group name along with the interest group owner and ad URL would make the user
// too identifiable. If this key is not k-anonymous then we do not provide the
// interest group name to reportWin.
std::string CONTENT_EXPORT
KAnonKeyForAdNameReporting(const blink::InterestGroup& group,
                           const blink::InterestGroup::Ad& ad);

// Manages k-anonymity updates. Checks last updated times in the database
// to limit updates (joins and queries) to once per day. Called by the
// InterestGroupManagerImpl for interest group k-anonymity updates. Calls
// The InterestGroupManagerImpl to access interest group storage to perform
// interest group updates.
class CONTENT_EXPORT InterestGroupKAnonymityManager {
 public:
  InterestGroupKAnonymityManager(
      InterestGroupManagerImpl* interest_group_manager,
      KAnonymityServiceDelegate* k_anonymity_service);
  ~InterestGroupKAnonymityManager();

  // Requests the k-anonymity status of elements of the interest group that
  // haven't been updated in 24 hours or more. Results are passed to
  // interest_group_manater_->UpdateKAnonymity.
  void QueryKAnonymityForInterestGroup(
      const StorageInterestGroup& storage_group);

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

  // An unowned pointer to the InterestGroupManagerImpl that owns this
  // InterestGroupUpdateManager. Used as an intermediary to talk to the
  // database.
  raw_ptr<InterestGroupManagerImpl> interest_group_manager_;

  raw_ptr<KAnonymityServiceDelegate> k_anonymity_service_;

  // We keep track of joins in progress because the joins that haven't completed
  // are still marked as eligible but it would be incorrect to join them
  // multiple times. We don't do this for query because the size of the request
  // could expose membership in overlapping groups through traffic analysis.
  base::flat_set<std::string> joins_in_progress;

  base::WeakPtrFactory<InterestGroupKAnonymityManager> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_K_ANONYMITY_MANAGER_H_
