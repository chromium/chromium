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
class CONTENT_EXPORT OriginTrialsControllerDelegate {
 public:
  virtual ~OriginTrialsControllerDelegate() = default;

  // Persist all enabled and persistable tokens in the |header_tokens|.
  // Subsequent calls to this method will overwrite the list of persisted trials
  // for the |origin|.
  virtual void PersistTrialsFromTokens(
      const url::Origin& origin,
      const base::span<const std::string> header_tokens,
      const base::Time current_time) = 0;

  // Returns |true| if |trial_name| has been persisted for |origin| and is still
  // valid. This method should be used by origin trial owners to check if the
  // feature under trial should be enabled.
  virtual bool IsTrialPersistedForOrigin(const url::Origin& origin,
                                         const base::StringPiece trial_name,
                                         const base::Time current_time) = 0;

  // Return the list of persistent origin trials that have been saved for
  // |origin| and haven't expired given the |current_time| parameter.
  virtual base::flat_set<std::string> GetPersistedTrialsForOrigin(
      const url::Origin& origin,
      base::Time current_time) = 0;

  // Remove all persisted tokens. Used to clear browsing data.
  virtual void ClearPersistedTokens() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ORIGIN_TRIALS_CONTROLLER_DELEGATE_H_
