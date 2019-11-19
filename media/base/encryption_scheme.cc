// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryption_scheme.h"

#include <ostream>

#include "base/logging.h"

namespace media {

std::ostream& operator<<(std::ostream& os, EncryptionScheme scheme) {
  switch (scheme) {
    case EncryptionScheme::kUnencrypted:
      return os << "Unencrypted";
    case EncryptionScheme::kCenc:
      return os << "CENC";
    case EncryptionScheme::kCbcs:
      return os << "CBCS";
    default:
      return os << "Unknown";
  }
}

}  // namespace media
