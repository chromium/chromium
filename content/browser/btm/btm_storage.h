// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_STORAGE_H_
#define CONTENT_BROWSER_BTM_BTM_STORAGE_H_

#include <cstddef>
#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/btm/btm_database.h"
#include "content/browser/btm/btm_state.h"
#include "content/browser/btm/btm_utils.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/clear_data_filter.mojom-forward.h"

class GURL;

namespace content {

// Manages the storage of `BtmState` values in the BTM database.
//
// This class is not thread-safe and should only be used on the sequence it was
// created on.
class CONTENT_EXPORT BtmStorage {
 public:
  explicit BtmStorage(const std::optional<base::FilePath>& path);
  ~BtmStorage();

  BtmState Read(const GURL& url);

  std::optional<PopupsStateValue> ReadPopup(const std::string& first_party_site,
                                            const std::string& tracking_site);

  std::vector<PopupWithTime> ReadRecentPopupsWithInteraction(
      const base::TimeDelta& lookback);

  bool WritePopup(const std::string& first_party_site,
                  const std::string& tracking_site,
                  const uint64_t access_id,
                  const base::Time& popup_time,
                  bool is_current_interaction,
                  bool is_authentication_interaction);

  void RemoveEvents(base::Time delete_begin,
                    base::Time delete_end,
                    network::mojom::ClearDataFilterPtr filter,
                    const BtmEventRemovalType type);

  // Delete all DB rows for |sites|.
  void RemoveRows(const std::vector<std::string>& sites);
  // Delete all DB rows for |sites| without a protective event. A protective
  // event is a user activation or successful WebAuthn assertion.
  void RemoveRowsWithoutProtectiveEvent(const std::set<std::string>& sites);

  // DIPS Helper Method Impls --------------------------------------------------

  // Record that there was a user activation on `url`.
  void RecordUserActivation(const GURL& url, base::Time time);
  void RecordWebAuthnAssertion(const GURL& url, base::Time time);
  // Record that |url| redirected the user.
  void RecordBounce(const GURL& url, base::Time time);

  // Storage querying Methods --------------------------------------------------

  // Returns two subsets of sites in `sites` with a protective event recorded.
  // A protective event is a user activation or successful WebAuthn assertion.
  //
  // The first item in the return value contains the sites that had a user
  // activation, and the second item contains the sites that had a WebAuthn
  // assertion.
  std::pair<std::set<std::string>, std::set<std::string>>
  FilterSitesWithProtectiveEvent(const std::set<std::string>& sites) const;

  // Returns the subset of sites in |sites| WITHOUT a protective event recorded.
  // A protective event is a user activation or successful WebAuthn assertion.
  std::set<std::string> FilterSitesWithoutProtectiveEvent(
      std::set<std::string> sites) const;

  // Returns all sites that did a bounce that aren't protected from DIPS.
  std::vector<std::string> GetSitesThatBounced(
      base::TimeDelta grace_period) const;

  // Returns the list of sites that should have their state cleared by BTM. How
  // these sites are determined is controlled by the value of
  // `features::kBtmTriggeringAction`. Passing a non-NULL `grace_period`
  // parameter overrides the use of `features::kBtmGracePeriod` when
  // evaluating sites to clear.
  std::vector<std::string> GetSitesToClear(
      std::optional<base::TimeDelta> grace_period) const;

  // Returns true if `url`'s site has had user activation since `bound`.
  bool DidSiteHaveUserActivationSince(const GURL& url, base::Time bound);

  // Returns the timestamp of the last user activation time on `url`, or
  // std::nullopt if there has been no user activation on `url`.
  std::optional<base::Time> LastUserActivationTime(const GURL& url);

  // Returns the timestamp of the last web authentication time on `url`, or
  // std::nullopt if there has been no authentication on `url`.
  std::optional<base::Time> LastWebAuthnAssertionTime(const GURL& url);

  // Returns the timestamp of the most recent of the last user activation or
  // authentication on `url`, or std::nullopt if there has been no user
  // activation or authentication on `url`.
  std::optional<base::Time> LastUserActivationOrAuthnAssertionTime(
      const GURL& url);

  // Returns time and type of the most recent interaction with the given url.
  std::pair<std::optional<base::Time>, BtmInteractionType>
  LastInteractionTimeAndType(const GURL& url);

  std::optional<base::Time> GetTimerLastFired();
  bool SetTimerLastFired(base::Time time);

  // Utility Methods -----------------------------------------------------------

  static void DeleteDatabaseFiles(base::FilePath path,
                                  base::OnceClosure on_complete);

  void SetClockForTesting(base::Clock* clock) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    db_->SetClockForTesting(clock);
  }

 protected:
  void Write(const BtmState& state);

 private:
  friend class BtmState;
  BtmState ReadSite(std::string site);

  std::unique_ptr<BtmDatabase> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BtmStorage> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_STORAGE_H_
