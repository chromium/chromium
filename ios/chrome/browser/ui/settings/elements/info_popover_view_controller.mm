// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/elements/info_popover_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"

namespace {

NSAttributedString* PrimaryMessage(NSString* full_text) {
  DCHECK(full_text);
  NSDictionary* general_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  return [[NSAttributedString alloc] initWithString:full_text
                                         attributes:general_attributes];
}

}  // namespace

@interface InfoPopoverViewController ()

@end

@implementation InfoPopoverViewController {
  // YES if it is presented by a UIButton.
  BOOL _isPresentingFromButton;
}

- (instancetype)initWithMessage:(NSString*)message {
  return [self initWithPrimaryAttributedString:PrimaryMessage(message)
                     secondaryAttributedString:nil
                                          icon:nil
                        isPresentingFromButton:YES];
}

- (instancetype)initWithPrimaryAttributedString:
                    (NSAttributedString*)primaryAttributedString
                      secondaryAttributedString:
                          (NSAttributedString*)secondaryAttributedString
                                           icon:(UIImage*)icon
                         isPresentingFromButton:(BOOL)isPresentingFromButton {
  self = [super initWithPrimaryAttributedString:primaryAttributedString
                      secondaryAttributedString:secondaryAttributedString
                                           icon:icon];
  if (self) {
    _isPresentingFromButton = isPresentingFromButton;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kEnterpriseInfoBubbleViewId;
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (void)popoverPresentationControllerDidDismissPopover:
    (UIPopoverPresentationController*)popoverPresentationController {
  if (_isPresentingFromButton) {
    UIButton* buttonView = base::apple::ObjCCastStrict<UIButton>(
        popoverPresentationController.sourceView);
    buttonView.enabled = YES;
  }
}

@end
