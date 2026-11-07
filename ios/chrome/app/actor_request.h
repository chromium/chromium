// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_ACTOR_REQUEST_H_
#define IOS_CHROME_APP_ACTOR_REQUEST_H_

#include <optional>
#include <string>

#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"

// Represents a parsed Remote Actor credential sharing request. `payload` and
// `nonce` are extracted from the incoming URL; `request` is populated once
// decrypted on a background sequence.
struct RemoteActorRequest {
  std::string payload;
  std::string nonce;
  std::optional<remote_actor::proto::RemoteActorPasswordUploadRequest> request;
};

#endif  // IOS_CHROME_APP_ACTOR_REQUEST_H_
