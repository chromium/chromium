// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/ui/reader_mode_view_controller.h"

#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface ReaderModeViewController ()

@end

@implementation ReaderModeViewController {
  UIView* _contentView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.accessibilityIdentifier = kReaderModeViewAccessibilityIdentifier;
}

- (void)willMoveToParentViewController:(UIViewController*)parent {
  if (!parent) {
    [self.view removeFromSuperview];
  }
  [super willMoveToParentViewController:parent];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent) {
    [parent.view addSubview:self.view];
    AddSameConstraints([NamedGuide guideWithName:kContentAreaGuide
                                            view:parent.view],
                       self.view);
  }
}

#pragma mark - ReaderModeConsumer

- (void)setContentView:(UIView*)contentView {
  if (_contentView) {
    [_contentView removeFromSuperview];
  }
  _contentView = contentView;
  if (_contentView) {
    _contentView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_contentView];
    AddSameConstraints(self.view, _contentView);
  }
}

@end
