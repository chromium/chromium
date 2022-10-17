// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/elements/instruction_view.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// URL for the Settings link.
const char* const kSettingsSyncURL = "internal://settings-sync";

}  // namespace

@implementation TangibleSyncViewController

@dynamic delegate;
@synthesize primaryIdentityAvatarImage = _primaryIdentityAvatarImage;
@synthesize primaryIdentityAvatarAccessibilityLabel =
    _primaryIdentityAvatarAccessibilityLabel;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.shouldHideBanner = YES;
  self.hasAvatarImage = YES;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);
  self.avatarImage = self.primaryIdentityAvatarImage;
  self.avatarAccessibilityLabel = self.primaryIdentityAvatarAccessibilityLabel;
  int titleStringID = 0;
  int subtitleStringID = 0;
  switch (fre_field_trial::GetNewMobileIdentityConsistencyFRE()) {
    case NewMobileIdentityConsistencyFRE::kTangibleSyncA:
      titleStringID = IDS_IOS_TANGIBLE_SYNC_TITLE_TURN_ON_SYNC;
      subtitleStringID = IDS_IOS_TANGIBLE_SYNC_SUBTITLE_BACK_UP;
      break;
    case NewMobileIdentityConsistencyFRE::kTangibleSyncB:
      titleStringID = IDS_IOS_TANGIBLE_SYNC_TITLE_SYNC;
      subtitleStringID = IDS_IOS_TANGIBLE_SYNC_SUBTITLE_BACK_UP;
      break;
    case NewMobileIdentityConsistencyFRE::kTangibleSyncC:
      titleStringID = IDS_IOS_TANGIBLE_SYNC_TITLE_TURN_ON_SYNC;
      subtitleStringID = IDS_IOS_TANGIBLE_SYNC_SUBTITLE_SYNC;
      break;
    case NewMobileIdentityConsistencyFRE::kTwoSteps:
    case NewMobileIdentityConsistencyFRE::kOld:
      NOTREACHED();
      break;
  }
  DCHECK_NE(0, titleStringID);
  DCHECK_NE(0, subtitleStringID);
  [self.delegate addConsentStringID:titleStringID];
  self.titleText = l10n_util::GetNSString(titleStringID);
  [self.delegate addConsentStringID:subtitleStringID];
  self.subtitleText = l10n_util::GetNSString(subtitleStringID);
  _activateSyncButtonID = IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON;
  [self.delegate addConsentStringID:_activateSyncButtonID];
  self.primaryActionString = l10n_util::GetNSString(_activateSyncButtonID);
  [self.delegate addConsentStringID:
                     IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION];
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECONDARY_ACTION);
  [self.delegate addConsentStringID:IDS_IOS_TANGIBLE_SYNC_DISCLAIMER];
  self.disclaimerText =
      l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_DISCLAIMER);
  self.disclaimerURLs = @[ net::NSURLWithGURL(GURL(kSettingsSyncURL)) ];
  [self.delegate addConsentStringID:IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_BOOKMARKS];
  [self.delegate addConsentStringID:IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_AUTOFILL];
  [self.delegate addConsentStringID:IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_HISTORY];
  NSArray<NSString*>* dataTypeNames = @[
    l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_BOOKMARKS),
    l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_AUTOFILL),
    l10n_util::GetNSString(IDS_IOS_TANGIBLE_SYNC_DATA_TYPE_HISTORY),
  ];
  // TODO(crbug.com/1363812): Need to icon version with custom symbols.
  NSArray<UIImage*>* dataTypeIcons = @[
    [UIImage imageNamed:@"tangible_sync_bookmarks"],
    [UIImage imageNamed:@"tangible_sync_autofill"],
    [UIImage imageNamed:@"tangible_sync_history"],
  ];
  InstructionView* instructionView =
      [[InstructionView alloc] initWithList:dataTypeNames
                                      style:InstructionViewStyleDefault
                                      icons:dataTypeIcons];
  instructionView.tapListener = self;
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.specificContentView addSubview:instructionView];
  [NSLayoutConstraint activateConstraints:@[
    [instructionView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [instructionView.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [instructionView.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
    [instructionView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];
  [super viewDidLoad];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.delegate logScrollButtonVisible:!self.didReachBottom];
}

#pragma mark - TangibleSyncConsumer

- (void)setPrimaryIdentityAvatarImage:(UIImage*)primaryIdentityAvatarImage {
  if (_primaryIdentityAvatarImage != primaryIdentityAvatarImage) {
    _primaryIdentityAvatarImage = primaryIdentityAvatarImage;
    self.avatarImage = primaryIdentityAvatarImage;
  }
}

- (void)setPrimaryIdentityAvatarAccessibilityLabel:
    (NSString*)primaryIdentityAvatarAccessibilityLabel {
  if (_primaryIdentityAvatarAccessibilityLabel !=
      primaryIdentityAvatarAccessibilityLabel) {
    _primaryIdentityAvatarAccessibilityLabel =
        primaryIdentityAvatarAccessibilityLabel;
    self.avatarAccessibilityLabel = primaryIdentityAvatarAccessibilityLabel;
  }
}

#pragma mark - InstructionLineTappedListener

// Sends histogram indicating that a line is tapped.
- (void)tappedOnLineNumber:(NSInteger)index {
  // TODO(crbug.com/1371062) Potentially open the settings menu
  signin_metrics::SigninSyncConsentDataRow enumIndex =
      signin_metrics::SigninSyncConsentDataRow::kBookmarksRowTapped;
  switch (index) {
    case 0:
      enumIndex = signin_metrics::SigninSyncConsentDataRow::kBookmarksRowTapped;
      break;
    case 1:
      enumIndex = signin_metrics::SigninSyncConsentDataRow::kAutofillRowTapped;
      break;
    case 2:
      enumIndex = signin_metrics::SigninSyncConsentDataRow::kHistoryRowTapped;
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramEnumeration("Signin.SyncConsentScreen.DataRowClicked",
                                enumIndex);
}

@end
