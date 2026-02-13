// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_action.h"

#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_action_type.h"

@implementation GeminiSettingsAction

- (instancetype)initWithType:(GeminiSettingsActionType)type
                         URL:(NSURL*)URL
              viewController:(UIViewController*)viewController {
  self = [super init];
  if (self) {
    switch (type) {
      case GeminiSettingsActionTypeViewController:
        if (!viewController || URL) {
          return nil;
        }
        break;
      case GeminiSettingsActionTypeURL:
        if (!URL || viewController) {
          return nil;
        }
        break;
      case GeminiSettingsActionTypeUnknown:
        break;
    }

    _type = type;
    _URL = URL;
    _viewController = viewController;
  }
  return self;
}

@end
