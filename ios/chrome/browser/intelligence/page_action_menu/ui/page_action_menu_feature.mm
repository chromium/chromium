// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_feature.h"

#import "base/check.h"

@implementation PageActionMenuFeature

- (instancetype)initWithFeatureType:(PageActionMenuFeatureType)featureType
                              title:(NSString*)title
                               icon:(UIImage*)icon
                         actionType:
                             (PageActionMenuFeatureActionType)actionType {
  DCHECK(title);
  DCHECK(icon);

  self = [super init];
  if (self) {
    _featureType = featureType;
    _title = [title copy];
    _icon = icon;
    _actionType = actionType;
    _toggleState = NO;
  }
  return self;
}

- (void)setSubtitle:(NSString*)subtitle {
  _subtitle = [subtitle copy];
}

- (void)setActionText:(NSString*)actionText {
  _actionText = [actionText copy];
}

- (void)setAccessibilityLabel:(NSString*)accessibilityLabel {
  _accessibilityLabel = [accessibilityLabel copy];
}

@end
