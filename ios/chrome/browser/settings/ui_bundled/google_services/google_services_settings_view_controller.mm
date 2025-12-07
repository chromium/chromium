// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/sync_switch_item.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/supervised_user_info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_view_controller_model_delegate.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface GoogleServicesSettingsViewController () <
    PopoverLabelViewControllerDelegate> {
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@property(nonatomic, strong) InfoPopoverViewController* bubbleViewController;

@end

@implementation GoogleServicesSettingsViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kGoogleServicesSettingsViewIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE);
  NSArray<UITrait>* traits = TraitCollectionSetForTraits(
      @[ UITraitPreferredContentSizeCategory.class ]);
  [self registerForTraitChanges:traits
                     withAction:@selector(closePopoverOnTraitChange)];
}

#pragma mark - Private

// Shows an info popover anchored on `buttonView` depending on the signed-in
// policy.
- (void)showManagedInfoPopoverOnButton:(UIButton*)buttonView
                 isForcedSigninEnabled:(BOOL)isForcedSigninEnabled {
  if (self.modelDelegate.isViewControllerSubjectToParentalControls) {
    self.bubbleViewController = [[SupervisedUserInfoPopoverViewController alloc]
        initWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_SUPERVISED_USER_UNAVAILABLE_SETTING_MESSAGE)];
  } else if (isForcedSigninEnabled) {
    self.bubbleViewController = [[EnterpriseInfoPopoverViewController alloc]
        initWithMessage:l10n_util::GetNSString(
                            IDS_IOS_ENTERPRISE_FORCED_SIGNIN_MESSAGE)
         enterpriseName:nil];
  } else {
    self.bubbleViewController = [[EnterpriseInfoPopoverViewController alloc]
        initWithEnterpriseName:nil];
  }

  self.bubbleViewController.delegate = self;
  // Disable the button when showing the bubble.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  self.bubbleViewController.popoverPresentationController.sourceView =
      buttonView;
  self.bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  self.bubbleViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionAny;

  [self presentViewController:self.bubbleViewController
                     animated:YES
                   completion:nil];
}

// Close popover when font size changed for accessibility. The font does not
// resize properly and the arrow is not aligned.
- (void)closePopoverOnTraitChange {
  if (!self.bubbleViewController) {
    return;
  }

  [self.bubbleViewController dismissViewControllerAnimated:YES completion:nil];
  UIButton* buttonView = base::apple::ObjCCastStrict<UIButton>(
      self.bubbleViewController.popoverPresentationController.sourceView);
  buttonView.enabled = YES;
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileGoogleServicesSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileGoogleServicesSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // No-op as there are no C++ objects or observers.

  _settingsAreDismissed = YES;
}

#pragma mark - GoogleServicesSettingsConsumer

- (void)reload {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  [self.tableView reloadData];
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  [self.modelDelegate googleServicesSettingsViewControllerLoadModel:self];
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self reloadData];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        googleServicesSettingsViewControllerDidRemove:self];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSGoogleServicesSettingsCloseWithSwipe"));
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

@end
