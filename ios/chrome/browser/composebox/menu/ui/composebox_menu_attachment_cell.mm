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

namespace {

// The ammount of padding added on the highlight of the pointer insteraction.
const CGFloat kPointerInteractionPadding = 8.0;

// The corner radius of the pointer highlight.
const CGFloat kPointerInteractionRadius = 20.0;

}  // namespace

@interface ComposeboxMenuAttachmentCell () <UIPointerInteractionDelegate>

@end

@implementation ComposeboxMenuAttachmentCell {
  ComposeboxMenuAttachmentView* _attachmentView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _attachmentView = [[ComposeboxMenuAttachmentView alloc] init];
    _attachmentView.translatesAutoresizingMaskIntoConstraints = NO;
    _attachmentView.userInteractionEnabled = NO;

    UIPointerInteraction* pointerInteraction =
        [[UIPointerInteraction alloc] initWithDelegate:self];
    [self addInteraction:pointerInteraction];

    [self.contentView addSubview:_attachmentView];
    AddSameConstraints(_attachmentView, self.contentView);
  }
  return self;
}

- (void)configureWithItem:(ComposeboxMenuItem*)item {
  _attachmentView.title = item.title;
  self.accessibilityLabel = item.title;

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

#pragma mark - UIPointerInteractionDelegate

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  // The preview APIs require the view to be in a window. Ensure they are before
  // proceeding.
  if (!self.window) {
    return nil;
  }

  UITargetedPreview* preview =
      [[UITargetedPreview alloc] initWithView:_attachmentView];
  UIPointerHighlightEffect* effect =
      [UIPointerHighlightEffect effectWithPreview:preview];

  CGRect highlightRegion =
      CGRectInset(_attachmentView.frame, -kPointerInteractionPadding,
                  -kPointerInteractionPadding);
  UIPointerShape* shape =
      [UIPointerShape shapeWithRoundedRect:highlightRegion
                              cornerRadius:kPointerInteractionRadius];
  return [UIPointerStyle styleWithEffect:effect shape:shape];
}

@end
