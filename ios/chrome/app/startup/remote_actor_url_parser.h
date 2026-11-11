// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_URL_PARSER_H_
#define IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_URL_PARSER_H_

#import <optional>

#import "ios/chrome/app/actor_request.h"

class GURL;

// Parses and decrypts a Remote Actor credential sharing request from `url`.
// Returns `std::nullopt` if the required query parameters are missing or if
// decryption/parsing fails.
std::optional<RemoteActorRequest> ParseRemoteActorRequestFromURL(
    const GURL& url);

#endif  // IOS_CHROME_APP_STARTUP_REMOTE_ACTOR_URL_PARSER_H_
