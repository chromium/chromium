// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_selection_placeholder_view_controller.h"

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_selection_delegate.h"
#import "url/gurl.h"

@implementation LensOverlaySelectionPlaceholderViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorWithWhite:1 alpha:0.7];

  UIButton* selectButton = [[UIButton alloc] init];
  UIButtonConfiguration* config =
      [UIButtonConfiguration borderedButtonConfiguration];
  config.title = @"Select";
  selectButton.configuration = config;
  [selectButton addTarget:self
                   action:@selector(selectButtonPressed)
         forControlEvents:UIControlEventTouchUpInside];

  [self.view addSubview:selectButton];
  selectButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [selectButton.heightAnchor constraintEqualToConstant:100.0f],
    [selectButton.widthAnchor constraintEqualToConstant:100.0f],
    [selectButton.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [selectButton.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
}

- (void)startFullImageRequestWithImage:(UIImage*)image {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [self.delegate
                       selectionUISuccessfullyCompletedFullImageRequest:self];
                 });
}

- (void)cancelOngoingRequests {
}

#pragma mark - UIActions

- (void)selectButtonPressed {
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [self.delegate selectionUI:self
                              performedSelection:nil
                       constructedResultsPageURL:GURL("http://chromium.org")
                                  suggestSignals:@"iil"];
                 });
}

@end
