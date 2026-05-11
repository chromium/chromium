// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_cell.h"

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_view.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation ComposeboxMenuAttachmentCell {
  ComposeboxMenuAttachmentView* _attachmentView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _attachmentView = [[ComposeboxMenuAttachmentView alloc] init];
    _attachmentView.translatesAutoresizingMaskIntoConstraints = NO;
    _attachmentView.userInteractionEnabled = NO;

    [self.contentView addSubview:_attachmentView];
    AddSameConstraints(_attachmentView, self.contentView);
  }
  return self;
}

- (void)configureWithItem:(ComposeboxMenuItem*)item {
  _attachmentView.title = item.title;
  _attachmentView.accessibilityLabel = item.title;

  self.accessibilityIdentifier =
      AccessibilityIdentifierForMenuItemType(item.type);

  if (item.disabled) {
    if (item.favicon) {
      _attachmentView.image = item.favicon;
    } else {
      _attachmentView.image = SymbolWithPalette(
          item.image, @[ [UIColor colorNamed:kTextSecondaryColor] ]);
    }
    _attachmentView.alpha = 0.5;
    self.userInteractionEnabled = NO;
    self.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
    self.isAccessibilityElement = YES;
  } else {
    if (item.favicon) {
      _attachmentView.image = item.favicon;
    } else {
      _attachmentView.image = SymbolWithPalette(
          item.image, @[ [UIColor colorNamed:kTextPrimaryColor] ]);
    }
    _attachmentView.alpha = 1.0;
    self.userInteractionEnabled = YES;
    self.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
    self.isAccessibilityElement = YES;
  }
}

@end
