// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Host used for Remote Actor credential sharing URLs.
extern NSString* const kRemoteActorURLHost;

// Query parameter keys for Remote Actor URL requests.
extern NSString* const kRemoteActorPayloadQueryParam;
extern NSString* const kRemoteActorNonceQueryParam;

// Base callback URL for returning to the Gemini iOS app after a Remote Actor
// credential sharing request finishes.
inline constexpr char kRemoteActorGeminiCallbackBaseURL[] =
    "googlegemini://agent-auth-result";

// Query parameter keys for the Remote Actor Gemini callback URL.
inline constexpr char kRemoteActorTaskIdQueryParam[] = "task_id";
inline constexpr char kRemoteActorStatusQueryParam[] = "status";
inline constexpr char kRemoteActorReasonQueryParam[] = "reason";

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_CONSTANTS_H_
