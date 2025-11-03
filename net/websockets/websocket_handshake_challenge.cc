// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_handshake_challenge.h"

#include "base/base64.h"
#include "crypto/obsolete/sha1.h"
#include "net/websockets/websocket_handshake_constants.h"

namespace net {

std::string ComputeSecWebSocketAccept(std::string_view key) {
  crypto::obsolete::Sha1 hash;
  hash.Update(key);
  hash.Update(websockets::kWebSocketGuid);
  return base::Base64Encode(hash.Finish());
}

}  // namespace net
