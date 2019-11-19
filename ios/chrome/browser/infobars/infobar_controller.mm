// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_controller.h"

#include <memory>

#include "base/logging.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfoBarController ()
@end

@implementation InfoBarController

@synthesize view = _view;
@synthesize delegate = _delegate;
@synthesize infoBarDelegate = _infoBarDelegate;
@synthesize infobarType = _infobarType;
@synthesize presented = _presented;
@synthesize hasBadge = _hasBadge;

#pragma mark - Public

- (instancetype)initWithInfoBarDelegate:
    (infobars::InfoBarDelegate*)infoBarDelegate {
  self = [super init];
  if (self) {
    _infoBarDelegate = infoBarDelegate;
    _presented = NO;
    _hasBadge = NO;
  }
  return self;
}

- (void)dealloc {
  [_view removeFromSuperview];
}

- (void)removeView {
  [_view removeFromSuperview];
}

- (void)detachView {
  _delegate = nullptr;
  _infoBarDelegate = nullptr;
}

#pragma mark - Properties

- (UIView*)view {
  if (!_view)
    _view = [self infobarView];
  return _view;
}

#pragma mark - Protected

- (UIView*)infobarView {
  NOTREACHED() << "Must be overriden in subclasses.";
  return _view;
}

- (BOOL)shouldIgnoreUserInteraction {
  // Ignore user interaction if view is already detached or is about to.
  return !_delegate || !_delegate->IsOwned() || !_infoBarDelegate;
}

@end
