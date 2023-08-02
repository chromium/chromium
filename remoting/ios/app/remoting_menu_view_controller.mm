// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/remoting_menu_view_controller.h"

#import <MaterialComponents/MaterialAppBar.h>
#import <MaterialComponents/MaterialButtons.h>

#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/string_resources.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/side_menu_items.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(nicholss): This should be generated from a remoting/base class:

static NSString* const kReusableIdentifierItem =
    @"remotingMenuViewControllerItem";
static UIColor* kBackgroundColor =
    [UIColor colorWithRed:0.f green:0.67f blue:0.55f alpha:1.f];

namespace {

// To use these scopes in a debug build, your development account will need to
// be allowlisted.
const char kChromotingAuthScopeValues[] =
    "https://www.googleapis.com/auth/chromoting.directory "
    "https://www.googleapis.com/auth/tachyon "
    "https://www.googleapis.com/auth/userinfo.email";

std::string GetAuthorizationCodeUri() {
  // Replace space characters with a '+' sign when formatting.
  bool use_plus = true;
  return base::StringPrintf(
      "https://accounts.google.com/o/oauth2/auth"
      "?scope=%s"
      "&redirect_uri=https://remotedesktop.google.com/_/oauthredirect"
      "&response_type=code"
      "&client_id=%s"
      "&access_type=offline"
      "&approval_prompt=force",
      base::EscapeUrlEncodedData(kChromotingAuthScopeValues, use_plus).c_str(),
      base::EscapeUrlEncodedData(
          google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
          use_plus)
          .c_str());
}

}  // namespace

@interface RemotingMenuViewController () {
  MDCAppBarViewController* _appBarViewController;
  NSArray<NSArray<SideMenuItem*>*>* _content;
}
@end

// This is the chromium version of the menu view controller. This will
// launch a web view to login and collect an oauth token to be able to login to
// the app without the standard google login flow. It also loads and shows all
// other menu items from SideMenuItemsProvider.
//
// The official app's implementation is in //ios_internal.
//
// Note: this class is not localized, it will not be shipped to production.
@implementation RemotingMenuViewController

- (id)init {
  self = [super init];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_SETTINGS_BUTTON);

    _appBarViewController = [[MDCAppBarViewController alloc] init];
    [self addChildViewController:_appBarViewController];

    _appBarViewController.headerView.backgroundColor = kBackgroundColor;
    _appBarViewController.navigationBar.tintColor = [UIColor whiteColor];
    _appBarViewController.navigationBar.titleTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor whiteColor]};
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _appBarViewController.headerView.trackingScrollView = self.collectionView;
  [self.view addSubview:_appBarViewController.view];
  [_appBarViewController didMoveToParentViewController:self];

  UIBarButtonItem* backButton =
      [[UIBarButtonItem alloc] initWithImage:RemotingTheme.backIcon
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(didTapBack:)];
  self.navigationItem.leftBarButtonItem = backButton;
  self.navigationItem.rightBarButtonItem = nil;

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forCellWithReuseIdentifier:kReusableIdentifierItem];

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
                 withReuseIdentifier:UICollectionElementKindSectionHeader];

  self.styler.cellStyle = MDCCollectionViewCellStyleCard;

  // Add the account management section to the beginning of the content.
  __weak __typeof(self) weakSelf = self;
  NSArray<SideMenuItem*>* accountManagementSection = @[
    [[SideMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_SIGN_IN_BUTTON)
                 icon:nil
               action:^{
                 [weakSelf didTapGetAccessCode];
               }],
    [[SideMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_SIGN_OUT_BUTTON)
                 icon:nil
               action:^{
                 [weakSelf didTapLogout];
               }]
  ];
  NSMutableArray<NSArray<SideMenuItem*>*>* mutableContent =
      [NSMutableArray arrayWithArray:SideMenuItemsProvider.sideMenuItems];
  [mutableContent insertObject:accountManagementSection atIndex:0];
  _content = mutableContent;
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return _content.count;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _content[section].count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  MDCCollectionViewTextCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kReusableIdentifierItem
                                forIndexPath:indexPath];
  SideMenuItem* item = _content[indexPath.section][indexPath.item];
  cell.textLabel.text = item.title;
  cell.imageView.image = item.icon;

  return cell;
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  MDCCollectionViewTextCell* supplementaryView =
      [collectionView dequeueReusableSupplementaryViewOfKind:kind
                                         withReuseIdentifier:kind
                                                forIndexPath:indexPath];
  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    if (indexPath.section == 0) {
      supplementaryView.textLabel.text = @"Account";
    }
    supplementaryView.textLabel.textColor = kBackgroundColor;
  }
  return supplementaryView;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  _content[indexPath.section][indexPath.item].action();
}

#pragma mark - <UICollectionViewDelegateFlowLayout>

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  // Only show the title if it's the account management section.
  if (section != 0) {
    return CGSizeZero;
  }
  return CGSizeMake(collectionView.bounds.size.width,
                    MDCCellDefaultOneLineHeight);
}

#pragma mark - Private

- (void)didTapBack:(id)button {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)didTapGetAccessCode {
  NSString* authUri = base::SysUTF8ToNSString(GetAuthorizationCodeUri());
  if (@available(iOS 10, *)) {
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:authUri]
                                       options:@{}
                             completionHandler:nil];
  }
#if !defined(__IPHONE_10_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
  else {
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:authUri]];
  }
#endif
}

- (void)didTapLogout {
  [RemotingService.instance.authentication logout];
}

@end
