// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_URL_PARSER_H_
#define IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_URL_PARSER_H_

#import <optional>

#import "ios/chrome/app/actor_request.h"

class GURL;

// Extracts the encrypted `payload` and `nonce` query parameters from `url`
// without accessing the Keychain. Returns `std::nullopt` if either query
// parameter is missing or empty.
std::optional<RemoteActorRequest> ExtractRemoteActorRequestFromURL(
    const GURL& url);

#endif  // IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_URL_PARSER_H_
