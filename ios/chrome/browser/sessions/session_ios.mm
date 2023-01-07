// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_ios.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/sessions/session_features.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
      base::mac::ObjCCast<NSArray<SessionWindowIOS*>>(
          [aDecoder decodeObjectForKey:kSessionWindowsKey]);

  return [self initWithWindows:(sessionWindows ? sessionWindows : @[])];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:_sessionWindows forKey:kSessionWindowsKey];
}

- (NSDictionary*)sessionTabContents {
  DCHECK(sessions::ShouldSaveSessionTabsToSeparateFiles());
  NSMutableDictionary* sessionContents = [[NSMutableDictionary alloc] init];
  for (SessionWindowIOS* sessionWindow : _sessionWindows) {
    [sessionContents addEntriesFromDictionary:sessionWindow.tabContents];
  }
  return sessionContents;
}

@end
