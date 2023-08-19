// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <stdint.h>

#import <string>

#import "ios/web/navigation/nscoder_util.h"

namespace web {
namespace nscoder_util {

void EncodeString(NSCoder* coder, NSString* key, const std::string& string) {
  [coder encodeBytes:reinterpret_cast<const uint8_t*>(string.data())
              length:string.size()
              forKey:key];
}

std::string DecodeString(NSCoder* decoder, NSString* key) {
  NSUInteger length = 0;
  const uint8_t* bytes = [decoder decodeBytesForKey:key returnedLength:&length];
  return std::string(reinterpret_cast<const char*>(bytes), length);
}

}  // namespace nscoder_util
}  // namespace web
