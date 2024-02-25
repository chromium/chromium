// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_account_details_view_controller.h"

#import "ios/chrome/browser/signin/model/system_identity.h"

@implementation FakeAccountDetailsViewController {
  __weak id<SystemIdentity> _identity;
  UITextView* _detailView;
}

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity {
  if ((self = [super init])) {
    _identity = identity;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Obnovioux color, this is a test screen.
  self.view.backgroundColor = [UIColor orangeColor];

  _detailView = [[UITextView alloc] init];
  _detailView.text =
      [NSString stringWithFormat:@"Details: %@", [_identity debugDescription]];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  const CGRect bounds = self.view.bounds;
  const CGPoint center =
      CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));

  [_detailView sizeToFit];
  _detailView.center = center;
}

@end
