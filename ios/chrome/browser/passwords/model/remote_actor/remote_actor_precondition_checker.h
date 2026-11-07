// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_PRECONDITION_CHECKER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_PRECONDITION_CHECKER_H_

#import <string_view>

#import "base/types/expected.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_callback_util.h"

class ProfileIOS;

// Precondition failure reasons checked before starting a Remote Actor
// credential sharing flow.
enum class RemoteActorPreconditionError {
  // Chrome has not completed the First Run Experience.
  kFirstRunIncomplete,
  // No user is currently signed in to the active profile.
  kNotSignedIn,
  // The requested GAIA ID does not match the active profile's GAIA ID.
  kProfileMismatch,
  // Enterprise policy blocks Remote Actor credential sharing.
  kBlockedByPolicy,
};

// Callback status and reason reported to Gemini when a precondition fails.
struct RemoteActorPreconditionCallbackParams {
  RemoteActorCallbackStatus status;
  RemoteActorCallbackReason reason;
};

// Validates that Chrome has completed FRE, `profile` is signed in, the active
// profile's primary GAIA ID matches `requested_gaia_id`, and enterprise policy
// permits Remote Actor credential sharing.
base::expected<void, RemoteActorPreconditionError>
CheckRemoteActorPreconditions(ProfileIOS* profile,
                              std::string_view requested_gaia_id);

// Maps a `RemoteActorPreconditionError` to the Gemini callback `status` and
// `reason`:
// - `kFirstRunIncomplete` / `kNotSignedIn` -> `kCancelled` / `kNotSignedIn`
// - `kProfileMismatch` -> `kFailed` / `kProfileMismatch`
// - `kBlockedByPolicy` -> `kFailed` / `kUploadFailed`
RemoteActorPreconditionCallbackParams GetPreconditionFailureCallbackParams(
    RemoteActorPreconditionError error);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_PRECONDITION_CHECKER_H_
