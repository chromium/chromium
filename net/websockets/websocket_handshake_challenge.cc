// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_challenge.h"

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "net/websockets/websocket_handshake_constants.h"

namespace net {

std::string ComputeSecWebSocketAccept(const std::string& key) {
  std::string accept;
  std::string hash = base::SHA1HashString(key + websockets::kWebSocketGuid);
  base::Base64Encode(hash, &accept);
  return accept;
}

}  // namespace net
