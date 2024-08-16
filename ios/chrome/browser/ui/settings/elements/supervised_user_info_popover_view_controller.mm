// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/elements/supervised_user_info_popover_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSAttributedString* PrimaryMessage(NSString* fullText) {
  DCHECK(fullText);
  NSDictionary* generalAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  return [[NSAttributedString alloc] initWithString:fullText
                                         attributes:generalAttributes];
}

NSAttributedString* SecondaryMessage(BOOL addLearnMoreLink) {
  // Create and format the text.
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };

  NSAttributedString* attributedString;
  if (addLearnMoreLink) {
    NSString* message = l10n_util::GetNSString(IDS_IOS_PARENT_MANAGED_SETTING);

    NSDictionary* linkAttributes = @{
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
      NSLinkAttributeName : [NSString
          stringWithUTF8String:supervised_user::kManagedByParentUiMoreInfoUrl],
    };

    attributedString = AttributedStringFromStringWithLink(
        message, textAttributes, linkAttributes);
  } else {
    attributedString = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(
                           IDS_IOS_PARENT_MANAGED_POPOVER_TEXT)
            attributes:textAttributes];
  }

  return attributedString;
}

}  // namespace

@interface SupervisedUserInfoPopoverViewController ()

@end

@implementation SupervisedUserInfoPopoverViewController

- (instancetype)initWithMessage:(NSString*)message {
  return [self initWithMessage:message
        isPresentingFromButton:YES
              addLearnMoreLink:YES];
}

- (instancetype)initWithMessage:(NSString*)message
         isPresentingFromButton:(BOOL)isPresentingFromButton
               addLearnMoreLink:(BOOL)addLearnMoreLink {
  return
      [super initWithPrimaryAttributedString:PrimaryMessage(message)
                   secondaryAttributedString:SecondaryMessage(addLearnMoreLink)
                                        icon:CustomSymbolWithPointSize(
                                                 kFamilylinkSymbol,
                                                 kSymbolAccessoryPointSize)
                      isPresentingFromButton:isPresentingFromButton];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kSupervisedUserInfoBubbleViewId;
}

@end
