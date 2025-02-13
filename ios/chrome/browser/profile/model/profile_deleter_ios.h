// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_DELETER_IOS_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_DELETER_IOS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

class ProfileIOS;

namespace ProfileDeleterIOS {

// Callback invoked with the result of trying to delete a profile.
using ProfileDeletedCallback = base::OnceCallback<void(std::string, bool)>;

// Deletes a `profile` that is already loaded, invoking `callback` with
// the success or failure state of the operation. The callback is called
// on the calling sequence.
void DeleteProfile(std::unique_ptr<ProfileIOS> profile,
                   ProfileDeletedCallback callback);

}  // namespace ProfileDeleterIOS

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_PROFILE_DELETER_IOS_H_
