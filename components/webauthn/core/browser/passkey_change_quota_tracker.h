// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_CHANGE_QUOTA_TRACKER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_CHANGE_QUOTA_TRACKER_H_

#include "base/containers/flat_map.h"
#include "components/sync/engine/cycle/commit_quota.h"
#include "url/origin.h"

namespace base {

template <typename T>
struct DefaultSingletonTraits;

}  // namespace base

namespace webauthn {

// PasskeyChangeQuotaTracker keeps a client-side quota for changes to
// `PasskeyModel` that may result in network traffic to Google and do not
// require user interaction.
// Quota is keyed by eTLD+1, and is global to a browser instance. Quota tracking
// is not kept after a browser restart.
// localhost and the virtual authenticator are exempt from the quota.
class PasskeyChangeQuotaTracker {
 public:
  static constexpr int kMaxTokensPerRP = 10;
  static constexpr base::TimeDelta kRefillInterval = base::Minutes(2);

  static PasskeyChangeQuotaTracker* GetInstance();

  ~PasskeyChangeQuotaTracker();
  PasskeyChangeQuotaTracker(const PasskeyChangeQuotaTracker&) = delete;
  PasskeyChangeQuotaTracker& operator=(const PasskeyChangeQuotaTracker&) =
      delete;

  // Tracks a new change by |origin|.
  void TrackChange(const url::Origin& origin);

  // Returns `true` if the quota hasn't been exceeded yet, and thus |origin| is
  // allowed to make that change, `false` if the change should be dropped as
  // quota has been exceeded.
  bool CanMakeChange(const url::Origin& origin);

 private:
  PasskeyChangeQuotaTracker();
  friend struct base::DefaultSingletonTraits<PasskeyChangeQuotaTracker>;

  // Returns the CommitQuota instance for |origin|, allocating it if necessary.
  syncer::CommitQuota& GetOrAllocateQuota(const url::Origin& origin);

  // A map tracking relying parties as eTLD+1 to their quota. If a relying party
  // is not present, it means it hasn't used any of its quota yet.
  base::flat_map<std::string, syncer::CommitQuota> quota_per_rp_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_CHANGE_QUOTA_TRACKER_H_
