// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encryption_scheme.h"

#include <ostream>


namespace media {

std::string GetEncryptionSchemeName(EncryptionScheme encryption_scheme) {
  switch (encryption_scheme) {
    case EncryptionScheme::kUnencrypted:
      return "Unencrypted";
    case EncryptionScheme::kCenc:
      return "CENC";
    case EncryptionScheme::kCbcs:
      return "CBCS";
    default:
      return "Unknown";
  }
}

std::ostream& operator<<(std::ostream& os, EncryptionScheme encryption_scheme) {
  return os << GetEncryptionSchemeName(encryption_scheme);
}

}  // namespace media
