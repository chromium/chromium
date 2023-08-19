// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_contained_presenter.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"

@implementation FakeContainedPresenter
@synthesize baseViewController = _baseViewController;
@synthesize presentedViewController = _presentedViewController;
@synthesize delegate = _delegate;
@synthesize lastPresentationWasAnimated = _lastPresentationWasAnimated;

- (void)prepareForPresentation {
  DCHECK(self.presentedViewController);
  DCHECK(self.baseViewController);

  [self.baseViewController addChildViewController:self.presentedViewController];
  [self.baseViewController.view addSubview:self.presentedViewController.view];

  [self.presentedViewController.view updateConstraints];
  [self.presentedViewController.view layoutIfNeeded];

  [self.presentedViewController
      didMoveToParentViewController:self.baseViewController];
}

- (void)presentAnimated:(BOOL)animated {
  self.lastPresentationWasAnimated = animated;
}

- (void)dismissAnimated:(BOOL)animated {
  DCHECK(self.presentedViewController);
  [self.presentedViewController willMoveToParentViewController:nil];
  [self.presentedViewController.view removeFromSuperview];
  [self.presentedViewController removeFromParentViewController];
  if ([self.delegate
          respondsToSelector:@selector(containedPresenterDidDismiss:)]) {
    [self.delegate containedPresenterDidDismiss:self];
  }
}

@end
