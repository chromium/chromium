// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/favicon_file_key.h"

#import <CommonCrypto/CommonCrypto.h>

#import <array>

#import "base/strings/string_util.h"

bool IsValidFaviconFileKey(NSString* key) {
  constexpr NSUInteger kFaviconKeyLength = CC_SHA256_DIGEST_LENGTH * 2;
  if (!key || key.length != kFaviconKeyLength) {
    return false;
  }
  std::array<unichar, kFaviconKeyLength> buffer;
  [key getCharacters:buffer.data() range:NSMakeRange(0, kFaviconKeyLength)];

  for (NSUInteger i = 0; i < kFaviconKeyLength; ++i) {
    if (!base::IsHexDigit(buffer[i])) {
      return false;
    }
  }
  return true;
}
