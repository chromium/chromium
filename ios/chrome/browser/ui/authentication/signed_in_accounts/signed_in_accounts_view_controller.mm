// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_view_controller.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_presentation_controller.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// Whether the Signed In Accounts view is currently being shown.
BOOL gSignedInAccountsViewControllerIsShown = NO;

}  // namespace

@interface SignedInAccountsViewController () <
    IdentityManagerObserverBridgeDelegate,
    UIViewControllerTransitioningDelegate> {
  ChromeBrowserState* _browserState;  // Weak.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  UILabel* _titleLabel;
  SignedInAccountsTableViewController* _accountTableView;
  UILabel* _infoLabel;
  UIButton* _primaryButton;
  UIButton* _secondaryButton;
}
@property(nonatomic, readonly, weak) id<ApplicationSettingsCommands> dispatcher;
@end

@implementation SignedInAccountsViewController
@synthesize dispatcher = _dispatcher;

+ (BOOL)shouldBePresentedForBrowserState:(ChromeBrowserState*)browserState {
  if (!browserState || browserState->IsOffTheRecord()) {
    return NO;
  }
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  return !gSignedInAccountsViewControllerIsShown &&
         authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin) &&
         !authService->IsAccountListApprovedByUser();
}

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                          dispatcher:
                              (id<ApplicationSettingsCommands>)dispatcher {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _browserState = browserState;
    _dispatcher = dispatcher;
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
  authService->ApproveAccountList();
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:completion];
}

- (void)dealloc {
  [_primaryButton removeTarget:self
                        action:@selector(onPrimaryButtonPressed:)
              forControlEvents:UIControlEventTouchDown];
  [_secondaryButton removeTarget:self
                          action:@selector(onSecondaryButtonPressed:)
                forControlEvents:UIControlEventTouchDown];
}

#pragma mark UIViewController

- (CGSize)preferredContentSize {
  CGFloat width = std::min(
      kDialogMaxWidth, self.presentingViewController.view.bounds.size.width -
                           2 * kViewControllerHorizontalPadding);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(_browserState);
  int shownAccounts =
      std::min(kMaxShownAccounts,
               identityManager->GetAccountsWithRefreshTokens().size());
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

  _accountTableView = [[SignedInAccountsTableViewController alloc]
      initWithBrowserState:_browserState];
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
  [_primaryButton addTarget:self
                     action:@selector(onPrimaryButtonPressed:)
           forControlEvents:UIControlEventTouchUpInside];
  NSString* primaryButtonTitle =
      l10n_util::GetNSString(IDS_IOS_SIGNED_IN_ACCOUNTS_VIEW_OK_BUTTON)
          .uppercaseString;
  [_primaryButton setTitle:primaryButtonTitle forState:UIControlStateNormal];
  _primaryButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  [_primaryButton setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                       forState:UIControlStateNormal];
  _primaryButton.titleLabel.font =
      [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
  _primaryButton.contentEdgeInsets = UIEdgeInsetsMake(8, 16, 8, 16);
  _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_primaryButton];

  _secondaryButton = [[UIButton alloc] init];
  [_secondaryButton addTarget:self
                       action:@selector(onSecondaryButtonPressed:)
             forControlEvents:UIControlEventTouchUpInside];
  NSString* secondaryButtonTitle =
      l10n_util::GetNSString(IDS_IOS_SIGNED_IN_ACCOUNTS_VIEW_SETTINGS_BUTTON)
          .uppercaseString;
  [_secondaryButton setTitle:secondaryButtonTitle
                    forState:UIControlStateNormal];
  [_secondaryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                         forState:UIControlStateNormal];
  _secondaryButton.titleLabel.font =
      [UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
  _secondaryButton.contentEdgeInsets = UIEdgeInsetsMake(8, 16, 8, 16);
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
  if ([self isBeingPresented] || [self isMovingToParentViewController]) {
    gSignedInAccountsViewControllerIsShown = YES;
  }
  [_accountTableView loadModel];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  if ([self isBeingDismissed] || [self isMovingFromParentViewController]) {
    gSignedInAccountsViewControllerIsShown = NO;
  }
}

#pragma mark Events

- (void)onPrimaryButtonPressed:(id)sender {
  [self dismissWithCompletion:nil];
}

- (void)onSecondaryButtonPressed:(id)sender {
  __weak id<ApplicationSettingsCommands> weakDispatcher = self.dispatcher;
  __weak UIViewController* weakPresentingViewController =
      self.presentingViewController;
  [self dismissWithCompletion:^{
    [weakDispatcher
        showAccountsSettingsFromViewController:weakPresentingViewController];
  }];
}

#pragma mark IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfRefreshTokenStateChanges {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(_browserState);
  if (identityManager->GetAccountsWithRefreshTokens().empty()) {
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
