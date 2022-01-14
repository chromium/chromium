// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/NSNumber+Permission.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation NSNumber (Permission)

+ (instancetype)cr_numberWithPermission:(web::Permission)value {
  return [self
      numberWithInt:static_cast<std::underlying_type<web::Permission>::type>(
                        value)];
}

- (web::Permission)cr_permissionValue {
  return static_cast<web::Permission>(self.intValue);
}

@end
