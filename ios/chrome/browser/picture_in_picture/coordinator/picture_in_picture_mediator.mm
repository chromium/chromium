// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/picture_in_picture/coordinator/picture_in_picture_mediator.h"

#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/promo/public/features.h"
#import "ios/chrome/browser/picture_in_picture/public/picture_in_picture_configuration.h"

@implementation PictureInPictureMediator {
  PictureInPictureConfiguration* _configuration;
}

- (instancetype)initWithConfiguration:
    (PictureInPictureConfiguration*)configuration {
  self = [super init];
  if (self) {
    _configuration = configuration;
  }
  return self;
}

#pragma mark - PictureInPictureMutator

- (void)startDestination {
  switch (_configuration.feature) {
    case PictureInPictureFeature::kDefaultBrowser:
      OpenIOSDefaultBrowserSettingsPage(IsDefaultAppsPictureInPictureVariant());
      break;
  }
}

#pragma mark - ButtonStackActionDelegate

- (void)didTapPrimaryActionButton {
  switch (_configuration.feature) {
    case PictureInPictureFeature::kDefaultBrowser:
      OpenIOSDefaultBrowserSettingsPage(IsDefaultAppsPictureInPictureVariant());
      break;
  }
}

- (void)didTapSecondaryActionButton {
  // Not used.
}

- (void)didTapTertiaryActionButton {
  // Not used.
}

@end
