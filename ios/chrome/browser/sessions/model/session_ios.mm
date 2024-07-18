// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_ios.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"

namespace {
// Serialization keys
NSString* const kSessionWindowsKey = @"sessionWindows";
}  // namespace

@implementation SessionIOS

@synthesize sessionWindows = _sessionWindows;

#pragma mark - Public interface.

- (instancetype)initWithWindows:(NSArray<SessionWindowIOS*>*)sessionWindows {
  DCHECK(sessionWindows);
  if ((self = [super init])) {
    _sessionWindows = sessionWindows;
  }
  return self;
}

#pragma mark - NSObject

- (instancetype)init {
  return [self initWithWindows:@[]];
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NSArray<SessionWindowIOS*>* sessionWindows =
      base::apple::ObjCCast<NSArray<SessionWindowIOS*>>(
          [aDecoder decodeObjectForKey:kSessionWindowsKey]);

  return [self initWithWindows:(sessionWindows ? sessionWindows : @[])];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:_sessionWindows forKey:kSessionWindowsKey];
}

@end
