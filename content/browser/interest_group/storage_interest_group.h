// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_STORAGE_INTEREST_GROUP_H_
#define CONTENT_BROWSER_INTEREST_GROUP_STORAGE_INTEREST_GROUP_H_

#include "content/common/content_export.h"

#include <vector>

#include "base/time/time.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// StorageInterestGroup contains both the auction worklet's Bidding interest
// group as well as several fields that are used by the browser process during
// an auction but are not needed by or should not be sent to the worklet
// process.
struct CONTENT_EXPORT StorageInterestGroup {
  StorageInterestGroup();
  StorageInterestGroup(StorageInterestGroup&&);
  StorageInterestGroup& operator=(StorageInterestGroup&&) = default;
  ~StorageInterestGroup();

  // KAnonymityData contains the information related to K-anonymity for either
  // an interest group or an ad. The interest groups are identified by update
  // URL or by owner and name. An ad's unique identifier is considered the ad or
  // ad component render URL. FLEDGE may not perform updates or report usage of
  // interest groups without a sufficiently large k. FLEDGE may not display ads
  // without a sufficiently large k.
  struct CONTENT_EXPORT KAnonymityData {
    bool operator==(const KAnonymityData& rhs) const {
      return key == rhs.key && is_k_anonymous == rhs.is_k_anonymous &&
             last_updated == rhs.last_updated;
    }

    // Unique identifier associated with the data being anonymized, usually a
    // URL.
    std::string key;
    // Whether the `key` was k-anonymous during the last update.
    bool is_k_anonymous;
    // The last time the unique user count was updated.
    base::Time last_updated;
  };

  blink::InterestGroup interest_group;
  auction_worklet::mojom::BiddingBrowserSignalsPtr bidding_browser_signals;

  // Can an ad bid (when k-anonymity is enforced)?
  std::vector<KAnonymityData> bidding_ads_kanon;
  // Can a component ad be part of a bid?
  std::vector<KAnonymityData> component_ads_kanon;
  // Can we report the interest group name for an ad?
  std::vector<KAnonymityData> reporting_ads_kanon;

  // Top level page origin from when the interest group was joined.
  url::Origin joining_origin;
  // Most recent time the interset group was joined. Stored in database as
  // `exact_join_time`.
  base::Time join_time;
  // The last time this interest group was updated.
  base::Time last_updated;
};

// Stream operator so KAnonymityData can be used in assertion statements.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const StorageInterestGroup::KAnonymityData& kanon);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_STORAGE_INTEREST_GROUP_H_
