// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NSCODER_UTIL_H_
#define IOS_WEB_NAVIGATION_NSCODER_UTIL_H_

#import <Foundation/Foundation.h>

#include <string>

namespace web {
namespace nscoder_util {

// Archives a std::string in an Objective-C key archiver.
void EncodeString(NSCoder* coder, NSString* key, const std::string& string);

// Decode a std::string from an Objective-C key unarchiver.
std::string DecodeString(NSCoder* decoder, NSString* key);

}  // namespace nscoder_util
}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NSCODER_UTIL_H_
