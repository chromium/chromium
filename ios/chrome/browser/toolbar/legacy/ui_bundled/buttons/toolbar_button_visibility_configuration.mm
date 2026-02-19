// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_button_visibility_configuration.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ui/base/device_form_factor.h"

@implementation ToolbarButtonVisibilityConfiguration

- (instancetype)initWithType:(ToolbarType)type {
  self = [super init];
  if (self) {
    _type = type;
  }
  return self;
}

- (ToolbarComponentVisibility)backButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityAlways &
             ~ToolbarComponentVisibilitySplit;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilitySplit;
  }
}

- (ToolbarComponentVisibility)forwardButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityAlways &
             ~ToolbarComponentVisibilitySplit;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilitySplit;
  }
}

- (ToolbarComponentVisibility)tabGridButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityAlways &
             ~ToolbarComponentVisibilitySplit;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilitySplit;
  }
}

- (ToolbarComponentVisibility)toolsMenuButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityAlways &
             ~ToolbarComponentVisibilitySplit;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilitySplit;
  }
}

- (ToolbarComponentVisibility)shareButtonVisibility {
  if (base::FeatureList::IsEnabled(kDisableShareButton) &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return ToolbarComponentVisibilityNone;
  }
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityAlways &
             ~ToolbarComponentVisibilitySplit;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilityNone;
  }
}

- (ToolbarComponentVisibility)reloadButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityAlways &
             ~ToolbarComponentVisibilitySplit;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilityNone;
  }
}

- (ToolbarComponentVisibility)stopButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityAlways &
             ~ToolbarComponentVisibilitySplit;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilityNone;
  }
}

- (ToolbarComponentVisibility)voiceSearchButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityRegularWidthRegularHeight;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilityNone;
  }
}

- (ToolbarComponentVisibility)contractButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityNone;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilityNone;
  }
}

- (ToolbarComponentVisibility)newTabButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityNone;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilitySplit;
  }
}

- (ToolbarComponentVisibility)locationBarLeadingButtonVisibility {
  switch (self.type) {
    case ToolbarType::kPrimary:
      return ToolbarComponentVisibilityAlways;
    case ToolbarType::kSecondary:
      return ToolbarComponentVisibilityNone;
  }
}

@end
