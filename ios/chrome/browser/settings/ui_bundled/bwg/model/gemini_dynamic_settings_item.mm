// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_dynamic_settings_item.h"

#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_action.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_action_type.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_metadata.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"

@implementation GeminiDynamicSettingsItem

- (instancetype)initWithType:(NSInteger)type
                    metadata:(GeminiSettingsMetadata*)metadata
                      action:(GeminiSettingsAction*)action {
  self = [super initWithType:type];
  if (self) {
    _metadata = metadata;
    _action = action;
    self.text = metadata.title;

    switch (action.type) {
      case GeminiSettingsActionTypeURL: {
        self.accessorySymbol =
            TableViewDetailTextCellAccessorySymbolExternalLink;
        self.accessibilityTraits |= UIAccessibilityTraitLink;
        break;
      }
      case GeminiSettingsActionTypeViewController: {
        self.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
        self.accessibilityTraits |= UIAccessibilityTraitButton;
        break;
      }
      case GeminiSettingsActionTypeUnknown: {
        self.accessoryType = UITableViewCellAccessoryNone;
        break;
      }
    }
  }
  return self;
}

@end
