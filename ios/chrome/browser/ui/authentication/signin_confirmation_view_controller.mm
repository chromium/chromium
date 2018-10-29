// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/signin_confirmation_view_controller.h"

#import "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#include "components/google/core/common/google_util.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/account_control_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/material_components/chrome_app_bar_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSigninConfirmationCollectionViewId =
    @"SigninConfirmationCollectionView";

namespace {
const CGFloat kAccountImageDimension = 64.;
const CGFloat kHeaderViewMinHeight = 170.;
const CGFloat kHeaderViewHeightMultiplier = 0.33;
const CGFloat kContentViewBottomInset = 40.;
// Leading separator inset.
const CGFloat kLeadingSeparatorInset = 30.;
// Trailing separator inset.
const CGFloat kTrailingSeparatorInset = 16.;

UIImage* GetImageForIdentity(ChromeIdentity* identity) {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetChromeIdentityService()
                       ->GetCachedAvatarForIdentity(identity);
  if (!image) {
    image = ios::GetChromeBrowserProvider()
                ->GetSigninResourcesProvider()
                ->GetDefaultAvatar();
    // No cached image, trigger a fetch, which will notify all observers
    // (including the corresponding AccountViewBase).
    ios::GetChromeBrowserProvider()
        ->GetChromeIdentityService()
        ->GetAvatarForIdentity(identity, ^(UIImage*){
                               });
  }
  return image;
}

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierInfo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSync = kItemTypeEnumZero,
  ItemTypeGoogleServices,
  ItemTypeFooter,
};
}

#pragma mark - SigninConfirmationViewController

@interface SigninConfirmationViewController ()<
    ChromeIdentityServiceObserver,
    CollectionViewFooterLinkDelegate> {
  ChromeIdentity* _identity;
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  __weak UIImage* _oldImage;
  UIImageView* _imageView;
  UILabel* _titleLabel;
  UILabel* _emailLabel;
  // List of string ids used for the user consent. The string ids order matches
  // the way they appear on the screen.
  std::vector<int> _consentStringIds;
}
@end

@implementation SigninConfirmationViewController

@synthesize delegate;

- (instancetype)initWithIdentity:(ChromeIdentity*)identity {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _identity = identity;
    _identityServiceObserver.reset(
        new ChromeIdentityServiceObserverBridge(self));
  }
  return self;
}

- (void)scrollToBottom {
  CGPoint bottomOffset = CGPointMake(
      0, self.collectionView.contentSize.height -
             self.collectionView.bounds.size.height + kContentViewBottomInset);
  [self.collectionView setContentOffset:bottomOffset animated:YES];
}

- (const std::vector<int>&)consentStringIds {
  return _consentStringIds;
}

- (int)openSettingsStringId {
  return IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OPEN_SETTINGS;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kSigninConfirmationCollectionViewId;

  // Configure the header.
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  headerView.canOverExtend = YES;
  headerView.maximumHeight = 200;
  headerView.shiftBehavior = MDCFlexibleHeaderShiftBehaviorEnabled;
  headerView.backgroundColor = [UIColor whiteColor];
  [headerView addSubview:[self contentViewWithFrame:headerView.bounds]];
  self.appBarViewController.navigationBar.hidesBackButton = YES;
  self.collectionView.backgroundColor = [UIColor clearColor];
  [headerView changeContentInsets:^{
    UIEdgeInsets contentInset = self.collectionView.contentInset;
    contentInset.bottom += kContentViewBottomInset;
    self.collectionView.contentInset = contentInset;
  }];

  // Load the contents of the collection view.
  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  BOOL didSend = [self
      sendDidReachBottomIfNecessary:self.collectionView.collectionViewLayout
                                        .collectionViewContentSize.height];
  if (!didSend && [self isMovingToParentViewController]) {
    // The confirmation screen just appeared and there wasn't enough space to
    // show the full screen (since the scroll hasn't reach the botton). This
    // means the "More" button is actually necessary.
    base::RecordAction(base::UserMetricsAction("Signin_MoreButton_Shown"));
  }
}

- (UIView*)contentViewWithFrame:(CGRect)frame {
  UIView* contentView = [[UIView alloc] initWithFrame:frame];
  contentView.autoresizingMask =
      (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);
  contentView.clipsToBounds = YES;
  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;

  _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _titleLabel.textColor = [[MDCPalette greyPalette] tint900];
  _titleLabel.font = [MDCTypography headlineFont];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  _emailLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _emailLabel.textColor = [[MDCPalette greyPalette] tint700];
  _emailLabel.font = [MDCTypography body1Font];
  _emailLabel.translatesAutoresizingMaskIntoConstraints = NO;

  [self updateViewWithIdentity:_identity];

  UIView* divider = [[UIView alloc] initWithFrame:CGRectZero];
  divider.backgroundColor = [[MDCPalette greyPalette] tint300];
  divider.translatesAutoresizingMaskIntoConstraints = NO;

  UILayoutGuide* layoutGuide1 = [[UILayoutGuide alloc] init];
  UILayoutGuide* layoutGuide2 = [[UILayoutGuide alloc] init];

  [contentView addSubview:_imageView];
  [contentView addSubview:_titleLabel];
  [contentView addSubview:_emailLabel];
  [contentView addSubview:divider];
  [contentView addLayoutGuide:layoutGuide1];
  [contentView addLayoutGuide:layoutGuide2];

  NSDictionary* views = @{
    @"image" : _imageView,
    @"title" : _titleLabel,
    @"email" : _emailLabel,
    @"divider" : divider,
    @"v1" : layoutGuide1,
    @"v2" : layoutGuide2
  };
  NSArray* constraints = @[
    @"V:[image]-(24)-[title]-(8)-[email]-(16)-[divider(==1)]|",
    @"H:|[v1][image]",
    @"H:|[v1(16)][title(<=440)][v2(>=v1)]|",
    @"H:|[v1][email]",
    @"H:|[divider]|",
  ];
  ApplyVisualConstraints(constraints, views);
  return contentView;
}

- (void)viewWillLayoutSubviews {
  CGSize viewSize = self.view.bounds.size;
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  headerView.maximumHeight =
      MAX(kHeaderViewMinHeight, kHeaderViewHeightMultiplier * viewSize.height);
}

- (void)updateViewWithIdentity:(ChromeIdentity*)identity {
  UIImage* identityImage = GetImageForIdentity(identity);
  if (_oldImage != identityImage) {
    _oldImage = identityImage;
    identityImage =
        ResizeImage(identityImage,
                    CGSizeMake(kAccountImageDimension, kAccountImageDimension),
                    ProjectionMode::kAspectFit);
    identityImage =
        CircularImageFromImage(identityImage, kAccountImageDimension);
    _imageView.image = identityImage;
  }

  _titleLabel.text = l10n_util::GetNSStringF(
      IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_TITLE,
      base::SysNSStringToUTF16([identity userFullName]));

  _emailLabel.text = [identity userEmail];
}

#pragma mark - Model

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  [model addSectionWithIdentifier:SectionIdentifierInfo];
  [model addItem:[self syncItem] toSectionWithIdentifier:SectionIdentifierInfo];
  [model addItem:[self googleServicesItem]
      toSectionWithIdentifier:SectionIdentifierInfo];
  [model addItem:[self openSettingsItem]
      toSectionWithIdentifier:SectionIdentifierInfo];
}

#pragma mark - Model items

- (CollectionViewItem*)syncItem {
  AccountControlItem* item =
      [[AccountControlItem alloc] initWithType:ItemTypeSync];
  item.text = [self localizedConsentStringWithId:
                        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SYNC_TITLE];
  item.detailText =
      [self localizedConsentStringWithId:
                IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SYNC_DESCRIPTION];
  item.image = ios::GetChromeBrowserProvider()
                   ->GetBrandedImageProvider()
                   ->GetSigninConfirmationSyncSettingsImage();
  return item;
}

- (CollectionViewItem*)googleServicesItem {
  AccountControlItem* item =
      [[AccountControlItem alloc] initWithType:ItemTypeGoogleServices];
  item.text =
      [self localizedConsentStringWithId:
                IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SERVICES_TITLE];
  item.detailText =
      [self localizedConsentStringWithId:
                IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SERVICES_DESCRIPTION];
  item.image = ios::GetChromeBrowserProvider()
                   ->GetBrandedImageProvider()
                   ->GetSigninConfirmationPersonalizeServicesImage();
  return item;
}

- (CollectionViewItem*)openSettingsItem {
  CollectionViewFooterItem* item =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  item.text = [self localizedConsentStringWithId:self.openSettingsStringId];
  item.linkURL = google_util::AppendGoogleLocaleParam(
      GURL("internal://settings-sync"),
      GetApplicationContext()->GetApplicationLocale());
  item.linkDelegate = self;
  return item;
}

#pragma mark - Helpers

// Calls |signinConfirmationControllerDidReachBottom:| on |delegate| if
// the collection view has reached its bottom based on a content size height of
// |contentSizeHeight|. Returns whether the delegate was notified.
- (BOOL)sendDidReachBottomIfNecessary:(CGFloat)contentSizeHeight {
  if (contentSizeHeight &&
      self.collectionView.contentOffset.y +
              self.collectionView.frame.size.height >=
          contentSizeHeight) {
    [self.delegate signinConfirmationControllerDidReachBottom:self];
    return YES;
  }
  return NO;
}

// Adds the string id to the list of string for the user consent, and returns
// the NSString related to the string id.
- (NSString*)localizedConsentStringWithId:(int)stringId {
  _consentStringIds.push_back(stringId);
  return l10n_util::GetNSString(stringId);
}

#pragma mark - ChromeIdentityServiceObserver

- (void)profileUpdate:(ChromeIdentity*)identity {
  if (identity == _identity) {
    [self updateViewWithIdentity:identity];
  }
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [super scrollViewDidScroll:scrollView];

  if (scrollView == self.collectionView) {
    [self sendDidReachBottomIfNecessary:scrollView.contentSize.height];
  }
}

#pragma mark UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  MDCCollectionViewCell* cell =
      [super collectionView:collectionView cellForItemAtIndexPath:indexPath];

  NSInteger itemType =
      [self.collectionViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeSync:
      cell.separatorInset = UIEdgeInsetsMake(0, kLeadingSeparatorInset, 0,
                                             kTrailingSeparatorInset);
      break;
    case ItemTypeGoogleServices:
      cell.shouldHideSeparator = YES;
      break;
    case ItemTypeFooter: {
      CollectionViewFooterCell* footerCell =
          base::mac::ObjCCastStrict<CollectionViewFooterCell>(cell);
      // TODO(crbug.com/664648): Must use atomic text formatting operation due
      // to LabelLinkController bug.
      footerCell.textLabel.attributedText = [[NSAttributedString alloc]
          initWithString:footerCell.textLabel.text
              attributes:@{
                NSFontAttributeName : [MDCTypography body1Font],
                NSForegroundColorAttributeName :
                    [[MDCPalette greyPalette] tint700]
              }];
      footerCell.horizontalPadding = 16;
      break;
    }

    default:
      break;
  }
  return cell;
}

#pragma mark - MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];

  // ItemTypeSync, ItemTypeGoogleServices, and ItemTypeFooter all support
  // dynamic height calculation.
  return [MDCCollectionViewCell
      cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                         forItem:item];
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(NSIndexPath*)indexPath {
  return YES;
}

- (BOOL)collectionView:(nonnull UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(nonnull NSIndexPath*)indexPath {
  return YES;
}

#pragma mark - CollectionViewFooterLinkDelegate

- (void)cell:(CollectionViewFooterCell*)cell didTapLinkURL:(GURL)URL {
  [[self delegate] signinConfirmationControllerDidTapSettingsLink:self];
}

@end
