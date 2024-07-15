// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_selection_placeholder_view_controller.h"

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_selection_delegate.h"
#import "url/gurl.h"

@interface LensOverlaySelectionPlaceholderViewController () {
  // Displays the received snapshot image.
  UIImageView* _snapshotImageView;

  // The received snapshot image.
  UIImage* _snapshot;

  // Constraint for the height of the snapshot image view.
  NSLayoutConstraint* _snapshotHeightConstraint;
}
@end

@implementation LensOverlaySelectionPlaceholderViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorWithWhite:1 alpha:0.7];

  [self createSnapshotImageView];

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
    [selectButton.topAnchor
        constraintEqualToAnchor:_snapshotImageView.bottomAnchor
                       constant:20.0f]
  ]];
}

- (void)createSnapshotImageView {
  _snapshotImageView = [[UIImageView alloc] init];
  _snapshotImageView.image = _snapshot;
  _snapshotImageView.contentMode = UIViewContentModeScaleToFill;
  _snapshotImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_snapshotImageView];

  _snapshotHeightConstraint = [_snapshotImageView.heightAnchor
      constraintEqualToAnchor:_snapshotImageView.widthAnchor];
  [NSLayoutConstraint activateConstraints:@[
    _snapshotHeightConstraint,
    [_snapshotImageView.widthAnchor constraintEqualToConstant:220.f],
    [_snapshotImageView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                                 constant:20.0f],
    [_snapshotImageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor]
  ]];

  [self updateImageViewHeight];
}

- (void)updateImageViewHeight {
  if (!_snapshotImageView || !_snapshot) {
    return;
  }
  CGFloat heightToWidthRatio = _snapshot.size.height / _snapshot.size.width;
  [_snapshotImageView removeConstraint:_snapshotHeightConstraint];
  _snapshotHeightConstraint = [_snapshotImageView.heightAnchor
      constraintEqualToAnchor:_snapshotImageView.widthAnchor
                   multiplier:heightToWidthRatio];
  _snapshotHeightConstraint.active = YES;
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
                       constructedResultsPageURL:
                           GURL("https://www.google.com/search?q=test&udm=2")
                                  suggestSignals:@"iil"];
                 });
}

#pragma mark - LensOverlaySnapshotConsumer

- (void)loadSnapshot:(UIImage*)snapshot {
  _snapshotImageView.image = snapshot;
  _snapshot = snapshot;

  [self updateImageViewHeight];
}

@end
