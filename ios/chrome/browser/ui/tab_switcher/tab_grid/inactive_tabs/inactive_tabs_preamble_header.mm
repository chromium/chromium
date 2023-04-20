// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_preamble_header.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Layout constants.
const CGFloat kTopPadding = 14;
const CGFloat kBottomPadding = 10;
const CGFloat kHorizontalPadding = 16;

}  // namespace

@interface InactiveTabsPreambleHeader () <UITextViewDelegate>
@end

@implementation InactiveTabsPreambleHeader {
  // The text view displaying the preamble text.
  UITextView* _textView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Use dark mode colors for this header's elements.
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

    _textView = CreateUITextViewWithTextKit1();
    _textView.scrollEnabled = NO;
    _textView.editable = NO;
    _textView.delegate = self;
    _textView.adjustsFontForContentSizeCategory = YES;
    _textView.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
    _textView.textContainer.lineFragmentPadding = 0;
    _textView.textContainerInset = UIEdgeInsets(
        kTopPadding, kHorizontalPadding, kBottomPadding, kHorizontalPadding);
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_textView];
    AddSameConstraints(_textView, self);
  }
  return self;
}

- (void)setDaysThreshold:(NSInteger)daysThreshold {
  DCHECK_NE(daysThreshold, kInactiveTabsDisabledByUser);
  _daysThreshold = daysThreshold;

  // Update the text view's attributed text.
  NSString* argument = [NSString stringWithFormat:@"%@", @(daysThreshold)];
  NSString* text =
      [L10nUtils formatStringForMessageID:IDS_IOS_INACTIVE_TABS_PREAMBLE
                                 argument:argument];
  NSDictionary* attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
  };
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    // Opening the link is handled by the delegate, so `NSLinkAttributeName`
    // can be arbitrary.
    NSLinkAttributeName : @"",
  };
  _textView.attributedText =
      AttributedStringFromStringWithLink(text, attributes, linkAttributes);
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK_EQ(textView, _textView);
  if (interaction == UITextItemInteractionInvokeDefaultAction &&
      _settingsLinkAction) {
    _settingsLinkAction();
  }
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  DCHECK_EQ(textView, _textView);
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text. Setting the `selectable` property to `NO` doesn't help
  // since it makes links inside the text view untappable.
  textView.selectedTextRange = nil;
}

@end
