// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/persistence/keychain.h"

#include "base/notreached.h"

namespace remoting {

// static
std::string Keychain::KeyToString(Key key) {
  switch (key) {
    case Key::REFRESH_TOKEN:
      return "RefreshToken";
    case Key::PAIRING_INFO:
      return "PairingInfo";
    default:
      NOTREACHED();
  }
}

// static
const std::string Keychain::kUnspecifiedAccount = "";

}  // namespace remoting
