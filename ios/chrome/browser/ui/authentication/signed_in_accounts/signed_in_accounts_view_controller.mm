// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_view_controller.h"

#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_presentation_controller.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_table_view_controller.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

const size_t kMaxShownAccounts = 3;
constexpr CGFloat kAccountsExtraBottomInset = 16;
constexpr CGFloat kVerticalPadding = 24;
constexpr CGFloat kButtonVerticalPadding = 16;
constexpr CGFloat kHorizontalPadding = 24;
constexpr CGFloat kAccountsHorizontalPadding = 8;
constexpr CGFloat kButtonHorizontalPadding = 16;
constexpr CGFloat kBetweenButtonsPadding = 8;
constexpr CGFloat kViewControllerHorizontalPadding = 20;
constexpr CGFloat kDialogMaxWidth = 328;
constexpr CGFloat kDefaultCellHeight = 54;

}  // namespace

@interface SignedInAccountsViewController () <
    IdentityManagerObserverBridgeDelegate,
    UIViewControllerTransitioningDelegate>
@end

@implementation SignedInAccountsViewController {
  ChromeBrowserState* _browserState;  // Weak.
  id<ApplicationSettingsCommands> _dispatcher;
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  UILabel* _titleLabel;
  SignedInAccountsTableViewController* _accountTableView;
  UILabel* _infoLabel;
  UIButton* _primaryButton;
  UIButton* _secondaryButton;
}

+ (BOOL)shouldBePresentedForBrowserState:(ChromeBrowserState*)browserState {
  if (!browserState || browserState->IsOffTheRecord()) {
    return NO;
  }
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  return authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin) &&
         !authService->IsAccountListApprovedByUser();
}

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                          dispatcher:
                              (id<ApplicationSettingsCommands>)dispatcher {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    CHECK(browserState);
    CHECK(dispatcher);
    _browserState = browserState;
    _dispatcher = dispatcher;
    _identityManager =
        IdentityManagerFactory::GetForBrowserState(_browserState);
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            IdentityManagerFactory::GetForBrowserState(_browserState), self);
    self.modalPresentationStyle = UIModalPresentationCustom;
    self.transitioningDelegate = self;
  }
  return self;
}

- (void)dismissWithCompletion:(ProceduralBlock)completion {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(_browserState);

  // `authService` expects the user to be signed in when approving the account
  // list (see crbug.com/1432369).
  if (authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    authService->ApproveAccountList();
  }
  __weak __typeof(self) weakSelf = self;
  [self.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf.delegate
                               signedInAccountsViewControllerIsDismissed:
                                   weakSelf];
                           if (completion) {
                             completion();
                           }
                         }];
}

- (void)teardownUI {
  if (!_browserState) {
    return;
  }
  [_accountTableView teardownUI];
  [_primaryButton removeTarget:self
                        action:@selector(onPrimaryButtonPressed:)
              forControlEvents:UIControlEventTouchDown];
  [_secondaryButton removeTarget:self
                          action:@selector(onSecondaryButtonPressed:)
                forControlEvents:UIControlEventTouchDown];
  _primaryButton = nil;
  _secondaryButton = nil;
  _identityManager = nullptr;
  _identityManagerObserver.reset();
  _dispatcher = nil;
  _browserState = nullptr;
}

#pragma mark UIViewController

- (CGSize)preferredContentSize {
  CGFloat width = std::min(
      kDialogMaxWidth, self.presentingViewController.view.bounds.size.width -
                           2 * kViewControllerHorizontalPadding);
  // Note (crbug.com/1472236#c2): |preferredContentSize| may be called by UIKit when
  // |_identityManger| is null (which from the code corresponds to a call after |teardownUI|).
  // Check if it is non-null before using it to avoid the crash.
  int shownAccounts =
      _identityManager
          ? std::min(kMaxShownAccounts,
                     _identityManager->GetAccountsWithRefreshTokens().size())
          : kMaxShownAccounts;
  CGSize maxSize = CGSizeMake(width - 2 * kHorizontalPadding, CGFLOAT_MAX);
  CGSize buttonSize = [_primaryButton sizeThatFits:maxSize];
  CGSize infoSize = [_infoLabel sizeThatFits:maxSize];
  CGSize titleSize = [_titleLabel sizeThatFits:maxSize];
  CGFloat height = kVerticalPadding + titleSize.height + kVerticalPadding +
                   shownAccounts * kDefaultCellHeight + kVerticalPadding +
                   infoSize.height + kVerticalPadding + buttonSize.height +
                   kButtonVerticalPadding;
  return CGSizeMake(width, height);
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_SIGNED_IN_ACCOUNTS_VIEW_TITLE);
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  const CGFloat kHeadlineFontSize = 24.0;
  const UIFontWeight kHeadlineFontWeight = UIFontWeightRegular;
  _titleLabel.font = [UIFont systemFontOfSize:kHeadlineFontSize
                                       weight:kHeadlineFontWeight];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_titleLabel];
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(_browserState);
  _accountTableView = [[SignedInAccountsTableViewController alloc]
      initWithIdentityManager:_identityManager
        accountManagerService:accountManagerService];
  _accountTableView.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_accountTableView];
  [self.view addSubview:_accountTableView.view];
  [_accountTableView didMoveToParentViewController:self];

  _infoLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _infoLabel.text =
      l10n_util::GetNSString(IDS_IOS_SIGNED_IN_ACCOUNTS_VIEW_INFO);
  _infoLabel.numberOfLines = 0;
  _infoLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  const CGFloat kBody1FontSize = 14.0;
  const UIFontWeight kBody1FontWeight = UIFontWeightRegular;
  _infoLabel.font = [UIFont systemFontOfSize:kBody1FontSize
                                      weight:kBody1FontWeight];
  _infoLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_infoLabel];

  _primaryButton = [[UIButton alloc] init];
  NSString* primaryButtonString =
      l10n_util::GetNSString(IDS_IOS_SIGNED_IN_ACCOUNTS_VIEW_OK_BUTTON);
  _primaryButton.accessibilityLabel = primaryButtonString;

  if (IsUIButtonConfigurationEnabled()) {
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets =
        NSDirectionalEdgeInsetsMake(8, 16, 8, 16);
    UIFont* font = [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSAttributedString* attributedTitle = [[NSAttributedString alloc]
        initWithString:primaryButtonString.uppercaseString
            attributes:attributes];
    buttonConfiguration.attributedTitle = attributedTitle;
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kSolidButtonTextColor];
    _primaryButton.configuration = buttonConfiguration;
  } else {
    UIEdgeInsets contentEdgeInsets = UIEdgeInsetsMake(8, 16, 8, 16);
    SetContentEdgeInsets(_primaryButton, contentEdgeInsets);
    [_primaryButton setTitle:primaryButtonString.uppercaseString
                    forState:UIControlStateNormal];
    [_primaryButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                         forState:UIControlStateNormal];
    _primaryButton.titleLabel.font =
        [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
  }

  [_primaryButton addTarget:self
                     action:@selector(onPrimaryButtonPressed:)
           forControlEvents:UIControlEventTouchUpInside];
  _primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_primaryButton];

  _secondaryButton = [[UIButton alloc] init];
  NSString* secondaryButtonString =
      l10n_util::GetNSString(IDS_IOS_SIGNED_IN_ACCOUNTS_VIEW_SETTINGS_BUTTON);
  _secondaryButton.accessibilityLabel = secondaryButtonString;

  if (IsUIButtonConfigurationEnabled()) {
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets =
        NSDirectionalEdgeInsetsMake(8, 16, 8, 16);
    UIFont* font = [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
    NSDictionary* attributes = @{NSFontAttributeName : font};
    NSAttributedString* attributedTitle = [[NSAttributedString alloc]
        initWithString:secondaryButtonString.uppercaseString
            attributes:attributes];
    buttonConfiguration.attributedTitle = attributedTitle;
    buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
    _secondaryButton.configuration = buttonConfiguration;
  } else {
    UIEdgeInsets contentEdgeInsets = UIEdgeInsetsMake(8, 16, 8, 16);
    SetContentEdgeInsets(_secondaryButton, contentEdgeInsets);
    [_secondaryButton setTitle:secondaryButtonString.uppercaseString
                      forState:UIControlStateNormal];
    [_secondaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                           forState:UIControlStateNormal];
    _secondaryButton.titleLabel.font =
        [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
  }

  [_secondaryButton addTarget:self
                       action:@selector(onSecondaryButtonPressed:)
             forControlEvents:UIControlEventTouchUpInside];
  _secondaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_secondaryButton];

  NSDictionary* views = @{
    @"title" : _titleLabel,
    @"accounts" : _accountTableView.view,
    @"info" : _infoLabel,
    @"primaryButton" : _primaryButton,
    @"secondaryButton" : _secondaryButton,
  };
  NSDictionary* metrics = @{
    @"verticalPadding" : @(kVerticalPadding),
    @"accountsVerticalPadding" :
        @(kVerticalPadding - kAccountsExtraBottomInset),
    @"buttonVerticalPadding" : @(kButtonVerticalPadding),
    @"horizontalPadding" : @(kHorizontalPadding),
    @"accountsHorizontalPadding" : @(kAccountsHorizontalPadding),
    @"buttonHorizontalPadding" : @(kButtonHorizontalPadding),
    @"betweenButtonsPadding" : @(kBetweenButtonsPadding),
  };
  NSArray* constraints = @[
    @"V:|-(verticalPadding)-[title]-(verticalPadding)-[accounts]",
    @"V:[accounts]-(accountsVerticalPadding)-[info]",
    @"V:[info]-(verticalPadding)-[primaryButton]-(buttonVerticalPadding)-|",
    @"V:[info]-(verticalPadding)-[secondaryButton]-(buttonVerticalPadding)-|",
    @"H:|-(horizontalPadding)-[title]-(horizontalPadding)-|",
    @"H:|-(accountsHorizontalPadding)-[accounts]-(accountsHorizontalPadding)-|",
    @"H:|-(horizontalPadding)-[info]-(horizontalPadding)-|",
    @"H:[secondaryButton]-(betweenButtonsPadding)-[primaryButton]",
    @"H:[primaryButton]-(buttonHorizontalPadding)-|",
  ];

  ApplyVisualConstraintsWithMetrics(constraints, views, metrics);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [_accountTableView loadModel];
}

#pragma mark Events

- (void)onPrimaryButtonPressed:(id)sender {
  [self dismissWithCompletion:nil];
}

- (void)onSecondaryButtonPressed:(id)sender {
  __weak id<ApplicationSettingsCommands> weakDispatcher = _dispatcher;
  __weak UIViewController* weakPresentingViewController =
      self.presentingViewController;
  [self dismissWithCompletion:^{
    [weakDispatcher
        showAccountsSettingsFromViewController:weakPresentingViewController];
  }];
}

#pragma mark IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfRefreshTokenStateChanges {
  if (_identityManager->GetAccountsWithRefreshTokens().empty()) {
    [self dismissWithCompletion:nil];
    return;
  }
  [_accountTableView loadModel];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  return [[SignedInAccountsPresentationController alloc]
      initWithPresentedViewController:presented
             presentingViewController:presenting];
}

@end
