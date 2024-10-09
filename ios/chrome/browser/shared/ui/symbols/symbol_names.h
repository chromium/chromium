// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_NAMES_H_
#define IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_NAMES_H_

#import <UIKit/UIKit.h>

#import "build/build_config.h"
#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"

/// *******
/// Import `symbols.h` and not this file directly.
/// *******

// Branded symbol names.
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
extern NSString* const kChromeDefaultBrowserIllustrationImage;
extern NSString* const kChromeDefaultBrowserScreenBannerImage;
extern NSString* const kChromeNotificationsOptInBannerImage;
extern NSString* const kChromeNotificationsOptInBannerLandscapeImage;
extern NSString* const kChromeSearchEngineChoiceIcon;
extern NSString* const kChromeSigninBannerImage;
extern NSString* const kChromeSigninPromoLogoImage;
extern NSString* const kGoogleDriveSymbol;
extern NSString* const kGoogleFullSymbol;
extern NSString* const kGoogleIconSymbol;
extern NSString* const kGoogleShieldSymbol;
extern NSString* const kGoogleMapsSymbol;
extern NSString* const kGooglePasswordManagerWidgetPromoImage;
extern NSString* const kGooglePasswordManagerWidgetPromoDisabledImage;
extern NSString* const kGooglePaySymbol;
extern NSString* const kGooglePhotosSymbol;
extern NSString* const kGooglePlusAddressSymbol;
extern NSString* const kGoogleSettingsPasswordsInOtherAppsBannerImage;
extern NSString* const kLensKeyboardAccessoryImage;
extern NSString* const kMulticolorChromeballSymbol;
extern NSString* const kPageInsightsSymbol;
extern NSString* const kFedexCarrierImage;
extern NSString* const kUPSCarrierImage;
extern NSString* const kUSPSCarrierImage;
#else
extern NSString* const kChromiumDefaultBrowserIllustrationImage;
extern NSString* const kChromiumDefaultBrowserScreenBannerImage;
extern NSString* const kChromiumNotificationsOptInBannerImage;
extern NSString* const kChromiumNotificationsOptInBannerLandscapeImage;
extern NSString* const kChromiumPasswordManagerWidgetPromoImage;
extern NSString* const kChromiumPasswordManagerWidgetPromoDisabledImage;
extern NSString* const kChromiumSearchEngineChoiceIcon;
extern NSString* const kChromiumSettingsPasswordsInOtherAppsBannerImage;
extern NSString* const kChromiumSigninBannerImage;
extern NSString* const kChromiumSigninPromoLogoImage;
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

// Custom symbol names.
extern NSString* const kPrivacySymbol;
extern NSString* const kSyncDisabledSymbol;
extern NSString* const kSafetyCheckSymbol;
extern NSString* const kArrowClockWiseSymbol;
extern NSString* const kIncognitoSymbol;
extern NSString* const kSquareNumberSymbol;
extern NSString* const kTranslateSymbol;
extern NSString* const kPasswordManagerSymbol;
extern NSString* const kEnterpriseSymbol;
extern NSString* const kPopupBadgeMinusSymbol;
extern NSString* const kPhotoSymbol;
extern NSString* const kPhotoBadgeArrowDownSymbol;
extern NSString* const kPhotoBadgePlusSymbol;
extern NSString* const kPhotoBadgeMagnifyingglassSymbol;
extern NSString* const kReadingListSymbol;
extern NSString* const kRecentTabsSymbol;
extern NSString* const kTabGroupsSymbol;
extern NSString* const kLanguageSymbol;
extern NSString* const kLocationSymbol;
extern NSString* const kPasswordSymbol;
#if !BUILDFLAG(IS_IOS_MACCATALYST)
extern NSString* const kMulticolorPasswordSymbol;
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
extern NSString* const kVoiceSymbol;
extern NSString* const kCameraLensSymbol;
extern NSString* const kDownTrendSymbol;
extern NSString* const kUpTrendSymbol;
extern NSString* const kShieldSymbol;
extern NSString* const kCloudSlashSymbol;
extern NSString* const kCloudAndArrowUpSymbol;
extern NSString* const kDinoSymbol;
extern NSString* const kChromeProductSymbol;
extern NSString* const kTunerSymbol;
extern NSString* const kMoveFolderSymbol;
extern NSString* const kTopOmniboxOptionSymbol;
extern NSString* const kBottomOmniboxOptionSymbol;
extern NSString* const kDangerousOmniboxSymbol;
extern NSString* const kArrowDownSymbol;
extern NSString* const kArrowUpSymbol;
extern NSString* const kFamilylinkSymbol;
extern NSString* const kMyDriveSymbol;
extern NSString* const kSharedDrivesSymbol;
extern NSString* const kEllipsisSquareFillSymbol;

// Custom symbol names which can be configured with a color palette. iOS 15+
// only.
extern NSString* const kIncognitoCircleFillSymbol;
extern NSString* const kPlusCircleFillSymbol;

// Custom symbols added for compatibility with iOS 15.0. These symbols are
// available as system symbols on iOS 15.1+.
extern NSString* const kCustomMovePlatterToBottomPhoneSymbol;
extern NSString* const kCustomMovePlatterToTopPhoneSymbol;

// Custom symbol to replace "palette" symbols on iOS 14. Cannot be used with a
// palette.
extern NSString* const kIncognitoCircleFilliOS14Symbol;

// Use custom symbol for camera because the default video icon in iOS should
// always represent “Apple Facetime”.
extern NSString* const kCameraSymbol;
extern NSString* const kCameraFillSymbol;

// Default symbol names.
extern NSString* const kChartBarXAxisSymbol;
extern NSString* const kCircleSymbol;
extern NSString* const kCircleFillSymbol;
extern NSString* const kSyncEnabledSymbol;
extern NSString* const kDefaultBrowserSymbol;
extern NSString* const kDefaultBrowseriOS14Symbol;
extern NSString* const kDiscoverSymbol;
extern NSString* const kBellSymbol;
extern NSString* const kBellSlashSymbol;
extern NSString* const kBellBadgeSymbol;
extern NSString* const kCachedDataSymbol;
extern NSString* const kAutofillDataSymbol;
extern NSString* const kSecureLocationBarSymbol;
extern NSString* const kNavigateToTabSymbol;
extern NSString* const kRefineQuerySymbol;
extern NSString* const kLinkActionSymbol;
extern NSString* const kQRCodeFinderActionSymbol;
extern NSString* const kNewTabActionSymbol;
extern NSString* const kPlusInCircleSymbol;
extern NSString* const kClipboardActionSymbol;
extern NSString* const kDeleteActionSymbol;
extern NSString* const kEditActionSymbol;
extern NSString* const kMarkAsUnreadActionSymbol;
extern NSString* const kMarkAsReadActionSymbol;
extern NSString* const kReadLaterActionSymbol;
extern NSString* const kAddBookmarkActionSymbol;
extern NSString* const kCopyActionSymbol;
extern NSString* const kPasteActionSymbol;
extern NSString* const kNewWindowActionSymbol;
extern NSString* const kShowActionSymbol;
extern NSString* const kHideActionSymbol;
extern NSString* const kFindInPageActionSymbol;
extern NSString* const kZoomTextActionSymbol;
extern NSString* const kSaveImageActionSymbol;
extern NSString* const kOpenImageActionSymbol;
extern NSString* const kQRCodeSymbol;
extern NSString* const kPrinterSymbol;
extern NSString* const kCreditCardSymbol;
extern NSString* const kMicrophoneFillSymbol;
extern NSString* const kMicrophoneSymbol;
extern NSString* const kMagnifyingglassSymbol;
extern NSString* const kMagnifyingglassCircleSymbol;
extern NSString* const kEllipsisCircleFillSymbol;
extern NSString* const kEllipsisRectangleSymbol;
extern NSString* const kPinSymbol;
extern NSString* const kPinSlashSymbol;
extern NSString* const kSettingsSymbol;
extern NSString* const kSettingsFilledSymbol;
extern NSString* const kShareSymbol;
extern NSString* const kXMarkSymbol;
extern NSString* const kXMarkCircleFillSymbol;
extern NSString* const kPlusSymbol;
extern NSString* const kSearchSymbol;
extern NSString* const kCheckmarkSymbol;
extern NSString* const kDownloadSymbol;
extern NSString* const kSecureSymbol;
extern NSString* const kWarningSymbol;
extern NSString* const kWarningFillSymbol;
extern NSString* const kHelpSymbol;
extern NSString* const kCheckmarkCircleSymbol;
extern NSString* const kCheckmarkCircleFillSymbol;
extern NSString* const kErrorCircleSymbol;
extern NSString* const kErrorCircleFillSymbol;
extern NSString* const kTrashSymbol;
extern NSString* const kInfoCircleSymbol;
extern NSString* const kHistorySymbol;
extern NSString* const kCheckmarkSealSymbol;
extern NSString* const kCheckmarkSealFillSymbol;
extern NSString* const kWifiSymbol;
extern NSString* const kBookmarksSymbol;
extern NSString* const kSyncErrorSymbol;
extern NSString* const kMenuSymbol;
extern NSString* const kSortSymbol;
extern NSString* const kExpandSymbol;
extern NSString* const kBackSymbol;
extern NSString* const kForwardSymbol;
extern NSString* const kPersonFillSymbol;
extern NSString* const kMailFillSymbol;
extern NSString* const kPhoneFillSymbol;
extern NSString* const kDownloadPromptFillSymbol;
extern NSString* const kDownloadDocFillSymbol;
extern NSString* const kDocSymbol;
extern NSString* const kOpenInDownloadsSymbol;
extern NSString* const kExternalLinkSymbol;
extern NSString* const kChevronDownSymbol;
extern NSString* const kChevronUpSymbol;
extern NSString* const kChevronBackwardSymbol;
extern NSString* const kChevronForwardSymbol;
extern NSString* const kChevronUpDown;
extern NSString* const kChevronDownCircleFill;
extern NSString* const kGlobeAmericasSymbol;
extern NSString* const kGlobeSymbol;
extern NSString* const kPersonCropCircleSymbol;
extern NSString* const kEqualSymbol;
extern NSString* const kBookClosedSymbol;
extern NSString* const kSunFillSymbol;
extern NSString* const kCalendarSymbol;
extern NSString* const kTabsSymbol;
extern NSString* const kHighlighterSymbol;
extern NSString* const kSealFillSymbol;
extern NSString* const kSquareOnSquareDashedSymbol;
extern NSString* const kDocPlaintextSymbol;
extern NSString* const kFlagSymbol;
extern NSString* const kKeyboardSymbol;
extern NSString* const kKeyboardDownSymbol;
extern NSString* const kSpeedometerSymbol;
extern NSString* const kMovePlatterToTopPhoneSymbol;
extern NSString* const kMovePlatterToBottomPhoneSymbol;
extern NSString* const kMapSymbol;
extern NSString* const kShippingBoxSymbol;
extern NSString* const kSliderHorizontalSymbol;
extern NSString* const kMacbookAndIPhoneSymbol;
extern NSString* const kCheckmarkShieldSymbol;
extern NSString* const kListBulletClipboardSymbol;
extern NSString* const kListBulletRectangleSymbol;
extern NSString* const kBoxTruckFillSymbol;
extern NSString* const kExclamationMarkBubbleSymbol;
extern NSString* const kShippingBoxFillSymbol;
extern NSString* const kButtonProgrammableSymbol;
extern NSString* const kCircleCircleFillSymbol;
extern NSString* const kLockSymbol;
extern NSString* const kRulerSymbol;
extern NSString* const kLaptopAndIphoneSymbol;
extern NSString* const kBoltSymbol;
extern NSString* const kLightBulbSymbol;
extern NSString* const kNewTabGroupActionSymbol;
extern NSString* const kRemoveTabFromGroupActionSymbol;
extern NSString* const kMoveTabToGroupActionSymbol;
extern NSString* const kClockSymbol;
extern NSString* const kUngroupTabGroupSymbol;
extern NSString* const kPlusInSquareSymbol;
extern NSString* const kMinusInCircleSymbol;
extern NSString* const kMultiIdentitySymbol;
extern NSString* const kStarBubbleFillSymbol;
extern NSString* const kTurnUpRightDiamondFillSymbol;
extern NSString* const kPencilSymbol;
extern NSString* const kMagicStackSymbol;
extern NSString* const kDiscoverFeedSymbol;
extern NSString* const kFilterSymbol;
extern NSString* const kSelectedFilterSymbol;
extern NSString* const kPersonTwoSymbol;
extern NSString* const kSquareFilledOnSquareSymbol;
extern NSString* const kPauseButton;
extern NSString* const kPlayButton;
extern NSString* const kFolderSymbol;

// Names of the default symbol being non-monochrome by default. When using them,
// you probably want to set their color to monochrome.
extern NSString* const kIPhoneSymbol;
extern NSString* const kIPadSymbol;
extern NSString* const kLaptopSymbol;
extern NSString* const kDesktopSymbol;

#endif  // IOS_CHROME_BROWSER_SHARED_UI_SYMBOLS_SYMBOL_NAMES_H_
