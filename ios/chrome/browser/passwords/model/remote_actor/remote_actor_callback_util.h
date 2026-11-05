// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CALLBACK_UTIL_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CALLBACK_UTIL_H_

#import <string_view>

#import "base/functional/callback.h"

// Overall status reported back to the Gemini iOS app in the Remote Actor
// callback URL.
enum class RemoteActorCallbackStatus {
  kSuccess,
  kCancelled,
  kFailed,
};

// Navigates back to the Gemini iOS app by opening the `googlegemini://`
// callback URL (`googlegemini://agent-auth-result`) populated with `task_id`
// and `status` via `UIApplication`. Invokes `completion` (which must be
// non-null) with `true` if the URL was successfully opened, or `false` if
// `task_id` is empty or the URL could not be opened. Must be called on the main
// thread.
void OpenGeminiCallbackURL(std::string_view task_id,
                           RemoteActorCallbackStatus status,
                           base::OnceCallback<void(bool)> completion);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CALLBACK_UTIL_H_
