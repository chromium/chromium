// Copyright 2022 The Chromium Authors
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

// BOOL that indicates if the view is still presented.
@property(nonatomic, assign) BOOL isPresented;

@end

@implementation OpenInActivityViewController

- (instancetype)initWithURL:(NSURL*)fileURL {
  NSArray* customActions = @[ fileURL ];
  NSArray* activities = nil;
  if (self = [super initWithActivityItems:customActions
                    applicationActivities:activities]) {
    self.fileURL = fileURL;
    __weak __typeof__(self) weakSelf = self;
    self.completionWithItemsHandler = ^(NSString*, BOOL, NSArray*, NSError*) {
      [weakSelf activityViewCompletionHandler];
    };
  }
  return self;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  self.isPresented = NO;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.isPresented = YES;
}

#pragma mark - Private Methods

// Invokes `openInActivityWillDisappearForFileAtURL:` when the view is about to
// be removed.
- (void)activityViewCompletionHandler {
  if (!self.isPresented) {
    [self.delegate openInActivityWillDisappearForFileAtURL:self.fileURL];
  }
}

@end
