// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_activity_view_controller.h"

#import "ios/chrome/browser/ui/open_in/open_in_activity_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OpenInActivityViewController ()

// NSURL of the presented file.
@property(nonatomic, strong) NSURL* fileURL;

@end

@implementation OpenInActivityViewController

- (instancetype)initWithURL:(NSURL*)fileURL {
  NSArray* customActions = @[ fileURL ];
  NSArray* activities = nil;
  if (self = [super initWithActivityItems:customActions
                    applicationActivities:activities]) {
    self.fileURL = fileURL;
  }
  return self;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.delegate openInActivityWillDisappearForFileAtURL:self.fileURL];
}

@end
