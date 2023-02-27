// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ORIGIN_TRIALS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_ORIGIN_TRIALS_CONTROLLER_DELEGATE_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// The |OriginTrialsControllerDelegate| interface exposes the functionality
// of the origin_trials component to the browser process.
//
// Use this class to check if a given persistent origin trial has been enabled
// for the current profile.
//
// See |components/origin_trials/README.md| for more information.
// TODO(https://crbug.com/1410180): Switch |partition_origin| to use Cookie
// partitioning. This interface uses the last committed origin from the
// outermost frame or document as partitioning as an interim measure to get a
// stable partitioning key until cookie partitioning is fully rolled out.
class CONTENT_EXPORT OriginTrialsControllerDelegate {
 public:
  virtual ~OriginTrialsControllerDelegate() = default;

  // Persist all enabled and persistable tokens in the |header_tokens|.
  //
  // Token persistence is partitioned based on |partition_origin|, meaning that
  // the storage keeps track of which |partition_origin|s have been seen when
  // persisting tokens for a given trial and origin.
  //
  // Subsequent calls to this method will update the registration of a token
  // for an origin. Passing an empty |header_tokens| will effectively clear the
  // persistence of tokens for the |origin| and |partition_origin|.
  // TODO(https://crbug.com/1410180): Switch |partition_origin| to use Cookie
  // partitioning.
  virtual void PersistTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      const base::span<const std::string> header_tokens,
      const base::Time current_time) = 0;

  // Appends all enabled and persistable |tokens| to the set of already enabled
  // trials for |origin|. By passing |script_origins|, this method can be used
  // to append third-party origin trials as well. If a token in |tokens| is a
  // third-party origin trial token, and the corresponding origin is present in
  // |script_tokens|, then the trial will be enabled for the origin stored in
  // the token itself, rather than any origin found in |script_origins|. This
  // limitation means that subdomain matching does not work for third-party
  // origin trial tokens using this method.
  virtual void PersistAdditionalTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      base::span<const url::Origin> script_origins,
      const base::span<const std::string> tokens,
      const base::Time current_time) = 0;

  // Returns |true| if |trial_name| has been persisted for |origin|,
  // partitioned by |partition_origin| and is still valid. This method should
  // be used by origin trial owners to check if the feature under trial should
  // be enabled.
  // TODO(https://crbug.com/1410180): Switch |partition_origin| to use Cookie
  // partitioning.
  virtual bool IsTrialPersistedForOrigin(const url::Origin& origin,
                                         const url::Origin& partition_origin,
                                         const base::StringPiece trial_name,
                                         const base::Time current_time) = 0;

  // Return the list of persistent origin trials that have been saved for
  // |origin|, partitioned by |partition_origin|, and haven't expired given the
  // |current_time| parameter.
  // TODO(https://crbug.com/1410180): Switch |partition_origin| to use Cookie
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
