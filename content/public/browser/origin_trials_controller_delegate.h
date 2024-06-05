// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ORIGIN_TRIALS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_ORIGIN_TRIALS_CONTROLLER_DELEGATE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/origin_trial_status_change_details.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "url/origin.h"

namespace content {

// The `OriginTrialsControllerDelegate` interface exposes the functionality
// of the origin_trials component to the browser process.
//
// Use this class to check if a given persistent origin trial has been enabled
// for the current profile.
//
// See `components/origin_trials/README.md` for more information.
// TODO(crbug.com/40254225): Switch `partition_origin` to use Cookie
// partitioning. This interface uses the last committed origin from the
// outermost frame or document as partitioning as an interim measure to get a
// stable partitioning key until cookie partitioning is fully rolled out.
class CONTENT_EXPORT OriginTrialsControllerDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the observed trial is enabled or disabled for
    // `details.origin` (under `details.partition_site`).
    //
    // NOTE: The status provided to this callback cannot be
    // guaranteed across startups, despite the observer being scoped to
    // persistent origin trials. Embedders that intend to use the
    // callbacks to inform some persisted setting should check those settings
    // against their associated trials on startup.
    // TODO (crbug.com/1466156): Verify that the call-sites for these methods
    // fully consider whether an active OriginTrialPolicy is disabling the
    // associated token, trial, and/or feature. Disabling one of these after a
    // persistent trial has previously been enabled for an origin should
    // effectively disabled also disable the trial for that origin.
    virtual void OnStatusChanged(
        const OriginTrialStatusChangeDetails& details) = 0;
    // Called when all persisted tokens are removed.
    virtual void OnPersistedTokensCleared() = 0;
    // The name of the persistent origin trial whose status changes `this`
    // is observing.
    virtual std::string trial_name() = 0;
  };

  virtual ~OriginTrialsControllerDelegate() = default;

  // Observers.
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}

  // Persist all enabled and persistable tokens in the `header_tokens`.
  //
  // Token persistence is partitioned based on `partition_origin`, meaning that
  // the storage keeps track of which `partition_origin`s have been seen when
  // persisting tokens for a given trial and origin.
  //
  // Subsequent calls to this method will update the registration of a token
  // for an origin. Passing an empty `header_tokens` will effectively clear the
  // persistence of tokens for the `origin` and `partition_origin`.
  // TODO(crbug.com/40254225): Switch `partition_origin` to use Cookie
  // partitioning.
  virtual void PersistTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) = 0;

  // Appends all enabled and persistable `tokens` to the set of already enabled
  // trials for `origin`. By passing `script_origins`, this method can be used
  // to append third-party origin trials as well. If a token in `tokens` is a
  // third-party origin trial token, and the corresponding origin is present in
  // `script_tokens`, then the trial will be enabled for the origin stored in
  // the token itself, rather than any origin found in `script_origins`.
  virtual void PersistAdditionalTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      base::span<const url::Origin> script_origins,
      const base::span<const std::string> tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) = 0;

  // Returns `true` if `feature` has been persistently enabled for `origin`,
  // partitioned by `partition_origin` and is still valid. This method should
  // be used by origin trial owners to check if the feature under trial should
  // be enabled.
  // TODO(crbug.com/40254225): Switch `partition_origin` to use Cookie
  // partitioning.
  virtual bool IsFeaturePersistedForOrigin(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      blink::mojom::OriginTrialFeature feature,
      const base::Time current_time) = 0;

  // Return the list of persistent origin trials that have been saved for
  // `origin`, partitioned by `partition_origin`, and haven't expired given the
  // `current_time` parameter.
  // TODO(crbug.com/40254225): Switch `partition_origin` to use Cookie
  // partitioning.
  virtual base::flat_set<std::string> GetPersistedTrialsForOrigin(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      base::Time current_time) = 0;

  // Remove all persisted tokens. Used to clear browsing data.
  virtual void ClearPersistedTokens() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ORIGIN_TRIALS_CONTROLLER_DELEGATE_H_
