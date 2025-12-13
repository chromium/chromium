// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation FaviconContentView {
  FaviconContentConfiguration* _configuration;
  FaviconContainerView* _faviconContainer;
  UIImageView* _badgeImageView;
}

- (instancetype)initWithConfiguration:
    (FaviconContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    _configuration = [configuration copy];
    _faviconContainer = [[FaviconContainerView alloc] init];
    _faviconContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_faviconContainer];

    _badgeImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _badgeImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_badgeImageView];

    AddSameConstraints(self, _faviconContainer);

    [NSLayoutConstraint activateConstraints:@[
      [_badgeImageView.centerXAnchor
          constraintEqualToAnchor:_faviconContainer.trailingAnchor],
      [_badgeImageView.centerYAnchor
          constraintEqualToAnchor:_faviconContainer.topAnchor],
    ]];
  }
  return self;
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return NO;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  _configuration =
      [base::apple::ObjCCastStrict<FaviconContentConfiguration>(configuration)
          copy];
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration isMemberOfClass:FaviconContentConfiguration.class];
}

#pragma mark - Private

// Updates the content view with the current configuration.
- (void)applyConfiguration {
  [_faviconContainer.faviconView
      configureWithAttributes:_configuration.faviconAttributes];
  _badgeImageView.image = _configuration.badgeImage;
  _badgeImageView.accessibilityIdentifier = _configuration.badgeAccessibilityID;
  _badgeImageView.hidden = !_configuration.badgeImage;
}

@end
