// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ENCRYPTION_SCHEME_H_
#define MEDIA_BASE_ENCRYPTION_SCHEME_H_

#include <iosfwd>

#include "media/base/media_export.h"

namespace media {

// The encryption mode. The definitions are from ISO/IEC 23001-7:2016.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.media
enum class EncryptionScheme {
  kUnencrypted = 0,
  kCenc,  // 'cenc' subsample encryption using AES-CTR mode.
  kCbcs,  // 'cbcs' pattern encryption using AES-CBC mode.
  kMaxValue = kCbcs
};

// For logging use only.
MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      EncryptionScheme encryption_scheme);

}  // namespace media

#endif  // MEDIA_BASE_ENCRYPTION_SCHEME_H_
