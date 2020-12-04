// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageViewController ()

// View controller representing the NTP content suggestions.
@property(nonatomic, strong) UIViewController* contentSuggestionsViewController;

@end

@implementation NewTabPageViewController

- (instancetype)initWithContentSuggestionsViewController:
    (UIViewController*)contentSuggestionsViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _contentSuggestionsViewController = contentSuggestionsViewController;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self addChildViewController:self.contentSuggestionsViewController];
  [self.view addSubview:self.contentSuggestionsViewController.view];
  [self.contentSuggestionsViewController didMoveToParentViewController:self];

  self.contentSuggestionsViewController.view
      .translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.contentSuggestionsViewController.view, self.view);
}

@end
