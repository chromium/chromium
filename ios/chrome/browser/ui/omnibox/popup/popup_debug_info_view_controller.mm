// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/popup_debug_info_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PopupDebugInfoViewController ()

@property(nonatomic, strong) UITextView* variationIDTextView;

@end

@implementation PopupDebugInfoViewController

- (instancetype)init {
  if (self = [super initWithNibName:nil bundle:nil]) {
    self.variationIDTextView = [[UITextView alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.variationIDTextView.editable = NO;
  self.variationIDTextView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.variationIDTextView];
  AddSameConstraints(self.view, self.variationIDTextView);

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonPressed)];
}

#pragma mark - PopupDebugInfoConsumer

- (void)setVariationIDString:(NSString*)string {
  self.variationIDTextView.text =
      [NSString stringWithFormat:@"Variation IDs:%@", string];
}

#pragma mark - private

- (void)doneButtonPressed {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

@end
