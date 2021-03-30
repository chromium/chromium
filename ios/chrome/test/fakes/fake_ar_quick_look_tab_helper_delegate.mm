// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_ar_quick_look_tab_helper_delegate.h"


#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FakeARQuickLookTabHelperDelegate ()

@property(nonatomic, strong) NSMutableArray* fileURLs;

@end

@implementation FakeARQuickLookTabHelperDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    _fileURLs = [NSMutableArray array];
  }
  return self;
}

- (void)ARQuickLookTabHelper:(ARQuickLookTabHelper*)tabHelper
    didFinishDowloadingFileWithURL:(NSURL*)fileURL
              allowsContentScaling:(BOOL)allowsScaling {
  [_fileURLs addObject:fileURL];
  _allowsContentScaling = allowsScaling;
}

@end
