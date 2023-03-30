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
const CGFloat kVerticalPadding = 8;
const CGFloat kHorizontalPadding = 16;

}  // namespace

@interface InactiveTabsPreambleHeader () <UITextViewDelegate>
@end

@implementation InactiveTabsPreambleHeader

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Use dark mode colors for this header's elements.
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;

    UITextView* textView = CreateUITextViewWithTextKit1();
    NSString* argument = InactiveTabsTimeThresholdDisplayString();
    NSString* text =
        [L10NUtils formatStringForMessageId:IDS_IOS_INACTIVE_TABS_PREAMBLE
                                   argument:argument];
    // Opening the link is handled by the delegate, so `NSLinkAttributeName`
    // can be arbitrary.
    NSDictionary* linkAttributes = @{NSLinkAttributeName : @""};
    textView.attributedText =
        AttributedStringFromStringWithLink(text, @{}, linkAttributes);
    textView.scrollEnabled = NO;
    textView.editable = NO;
    textView.delegate = self;
    textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    textView.adjustsFontForContentSizeCategory = YES;
    textView.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
    textView.textColor = [UIColor colorNamed:kTextSecondaryColor];
    textView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    textView.textContainer.lineFragmentPadding = 0;
    textView.textContainerInset =
        UIEdgeInsets(kVerticalPadding, kHorizontalPadding, kVerticalPadding,
                     kHorizontalPadding);
    textView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:textView];
    AddSameConstraints(textView, self);
  }
  return self;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (interaction == UITextItemInteractionInvokeDefaultAction &&
      _settingsLinkAction) {
    _settingsLinkAction();
  }
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text. Setting the `selectable` property to `NO` doesn't help
  // since it makes links inside the text view untappable.
  textView.selectedTextRange = nil;
}

@end
