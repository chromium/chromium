// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/remote_actor_url_parser.h"

#import <string>
#import <utility>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_constants.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

std::optional<RemoteActorRequest> ExtractRemoteActorRequestFromURL(
    const GURL& url) {
  std::string payload;
  std::string nonce;
  if (!net::GetValueForKeyInQuery(
          url, base::SysNSStringToUTF8(kRemoteActorPayloadQueryParam),
          &payload) ||
      !net::GetValueForKeyInQuery(
          url, base::SysNSStringToUTF8(kRemoteActorNonceQueryParam), &nonce) ||
      payload.empty() || nonce.empty()) {
    return std::nullopt;
  }

  return RemoteActorRequest{
      .payload = std::move(payload),
      .nonce = std::move(nonce),
  };
}
