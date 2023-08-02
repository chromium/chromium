// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_ar_quick_look_tab_helper_delegate.h"

@implementation FakeARQuickLookTabHelperDelegate {
  NSMutableArray<NSURL*>* _fileURLs;
  NSURL* _canonicalURL;
  BOOL _allowsContentScaling;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _fileURLs = [NSMutableArray array];
  }
  return self;
}

- (void)presentUSDZFileWithURL:(NSURL*)fileURL
                  canonicalURL:(NSURL*)canonicalURL
                      webState:(web::WebState*)webState
           allowContentScaling:(BOOL)allowContentScaling {
  [_fileURLs addObject:fileURL];
  _allowsContentScaling = allowContentScaling;
  _canonicalWebPageURL = canonicalURL;
}

@end
