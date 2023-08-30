// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/symbol_names.h"

// Custom symbol names.
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
NSString* const kGoogleIconSymbol = @"google_icon";
NSString* const kGoogleShieldSymbol = @"google_shield";
NSString* const kChromeSymbol = @"chrome_symbol";
NSString* const kGoogleMapsSymbol = @"google_maps";
NSString* const kGooglePhotosSymbol = @"google_photos";
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
NSString* const kPrivacySymbol = @"checkerboard_shield";
NSString* const kSyncDisabledSymbol = @"arrow_triangle_slash_circlepath";
NSString* const kSafetyCheckSymbol = @"checkermark_shield";
NSString* const kArrowClockWiseSymbol = @"arrow_clockwise";
NSString* const kIncognitoSymbol = @"incognito";
NSString* const kSquareNumberSymbol = @"square_number";
NSString* const kTranslateSymbol = @"translate";
NSString* const kCameraSymbol = @"camera";
NSString* const kCameraFillSymbol = @"camera_fill";
NSString* const kPasswordManagerSymbol = @"password_manager";
NSString* const kPopupBadgeMinusSymbol = @"popup_badge_minus";
NSString* const kPhotoBadgePlusSymbol = @"photo_badge_plus";
NSString* const kPhotoBadgeMagnifyingglassSymbol =
    @"photo_badge_magnifyingglass";
NSString* const kReadingListSymbol = @"square_bullet_square";
NSString* const kRecentTabsSymbol = @"laptopcomputer_and_phone";
NSString* const kLanguageSymbol = @"language";
NSString* const kLocationSymbol = @"location";
NSString* const kLocationFillSymbol = @"location.fill";
NSString* const kPasswordSymbol = @"password";
#if !BUILDFLAG(IS_IOS_MACCATALYST)
NSString* const kMulticolorPasswordSymbol = @"multicolor_password";
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
NSString* const kCameraLensSymbol = @"camera_lens";
NSString* const kDownTrendSymbol = @"line_downtrend";
NSString* const kIncognitoCircleFilliOS14Symbol =
    @"incognito_circle_fill_ios14";
NSString* const kShieldSymbol = @"shield";
NSString* const kCloudSlashSymbol = @"cloud_slash";
NSString* const kCloudAndArrowUpSymbol = @"cloud_and_arrow_up";
NSString* const kDinoSymbol = @"dino";
NSString* const kChromeProductSymbol = @"chrome_product";
NSString* const kTunerSymbol = @"tuner";
NSString* const kMoveFolderSymbol = @"folder_badge_arrow_forward";
NSString* const kTopOmniboxOptionSymbol = @"top_omnibox_option";
NSString* const kBottomOmniboxOptionSymbol = @"bottom_omnibox_option";
NSString* const kDangerousOmniboxSymbol = @"dangerous_omnibox";

// Custom symbol names which can be configured with a color palette.
NSString* const kIncognitoCircleFillSymbol = @"incognito_circle_fill";
NSString* const kPlusCircleFillSymbol = @"plus_circle_fill";

// Custom symbols added for compatibility with iOS 15.0. These symbols are
// available as system symbols on iOS 15.1+.
NSString* const kCustomMovePlatterToBottomPhoneSymbol =
    @"custom_platter_filled_bottom_and_arrow_down_iphone";
NSString* const kCustomMovePlatterToTopPhoneSymbol =
    @"custom_platter_filled_top_and_arrow_up_iphone";

// Default symbol names.
NSString* const kChartBarXAxisSymbol = @"chart.bar.xaxis";
NSString* const kCircleSymbol = @"circle";
NSString* const kCircleFillSymbol = @"circle.fill";
NSString* const kSyncEnabledSymbol = @"arrow.triangle.2.circlepath";
NSString* const kSyncCircleSymbol = @"arrow.triangle.2.circlepath.circle.fill";
NSString* const kDefaultBrowserSymbol = @"app.badge.checkmark";
NSString* const kDefaultBrowseriOS14Symbol = @"app.badge";
NSString* const kDiscoverSymbol = @"flame";
NSString* const kBellSymbol = @"bell";
NSString* const kCachedDataSymbol = @"photo.on.rectangle";
NSString* const kAutofillDataSymbol = @"wand.and.rays";
NSString* const kSecureLocationBarSymbol = @"lock.fill";
NSString* const kNavigateToTabSymbol = @"arrow.right.circle";
NSString* const kRefineQuerySymbol = @"arrow.up.backward";
NSString* const kLinkActionSymbol = @"link";
NSString* const kQRCodeFinderActionSymbol = @"qrcode.viewfinder";
NSString* const kNewTabActionSymbol = @"plus.square";
NSString* const kNewTabCircleActionSymbol = @"plus.circle";
NSString* const kClipboardActionSymbol = @"doc.on.clipboard";
NSString* const kDeleteActionSymbol = @"trash";
NSString* const kEditActionSymbol = @"pencil";
NSString* const kMarkAsUnreadActionSymbol = @"text.badge.minus";
NSString* const kMarkAsReadActionSymbol = @"text.badge.checkmark";
NSString* const kReadLaterActionSymbol = @"text.badge.plus";
NSString* const kAddBookmarkActionSymbol = @"star";
NSString* const kCopyActionSymbol = @"doc.on.doc";
NSString* const kPasteActionSymbol = @"doc.on.clipboard";
NSString* const kNewWindowActionSymbol = @"square.split.2x1";
NSString* const kShowActionSymbol = @"eye";
NSString* const kHideActionSymbol = @"eye.slash";
NSString* const kFindInPageActionSymbol = @"doc.text.magnifyingglass";
NSString* const kZoomTextActionSymbol = @"plus.magnifyingglass";
NSString* const kSaveImageActionSymbol = @"square.and.arrow.down";
NSString* const kOpenImageActionSymbol = @"arrow.up.right.square";
NSString* const kQRCodeSymbol = @"qrcode";
NSString* const kPrinterSymbol = @"printer";
NSString* const kCreditCardSymbol = @"creditcard";
NSString* const kMicrophoneFillSymbol = @"mic.fill";
NSString* const kMicrophoneSymbol = @"mic";
NSString* const kMagnifyingglassSymbol = @"magnifyingglass";
NSString* const kMagnifyingglassCircleSymbol = @"magnifyingglass.circle";
NSString* const kEllipsisCircleFillSymbol = @"ellipsis.circle.fill";
NSString* const kEllipsisRectangleSymbol = @"ellipsis.rectangle";
NSString* const kPinSymbol = @"pin";
NSString* const kPinSlashSymbol = @"pin.slash";
NSString* const kSettingsSymbol = @"gearshape";
NSString* const kSettingsFilledSymbol = @"gearshape.fill";
NSString* const kShareSymbol = @"square.and.arrow.up";
NSString* const kXMarkSymbol = @"xmark";
NSString* const kXMarkCircleFillSymbol = @"xmark.circle.fill";
NSString* const kPlusSymbol = @"plus";
NSString* const kSearchSymbol = @"magnifyingglass";
NSString* const kCheckmarkSymbol = @"checkmark";
NSString* const kDownloadSymbol = @"arrow.down.circle";
NSString* const kWarningSymbol = @"exclamationmark.triangle";
NSString* const kWarningFillSymbol = @"exclamationmark.triangle.fill";
NSString* const kHelpSymbol = @"questionmark.circle";
NSString* const kCheckmarkCircleSymbol = @"checkmark.circle";
NSString* const kCheckmarkCircleFillSymbol = @"checkmark.circle.fill";
NSString* const kErrorCircleSymbol = @"exclamationmark.circle";
NSString* const kErrorCircleFillSymbol = @"exclamationmark.circle.fill";
NSString* const kTrashSymbol = @"trash";
NSString* const kInfoCircleSymbol = @"info.circle";
NSString* const kHistorySymbol = @"clock.arrow.circlepath";
NSString* const kCheckmarkSealSymbol = @"checkmark.seal";
NSString* const kCheckmarkSealFillSymbol = @"checkmark.seal.fill";
NSString* const kWifiSymbol = @"wifi";
NSString* const kBookmarksSymbol = @"star";
NSString* const kSyncErrorSymbol =
    @"exclamationmark.arrow.triangle.2.circlepath";
NSString* const kMenuSymbol = @"ellipsis";
NSString* const kSortSymbol = @"arrow.up.arrow.down";
NSString* const kBackSymbol = @"arrow.backward";
NSString* const kForwardSymbol = @"arrow.forward";
NSString* const kPersonFillSymbol = @"person.fill";
NSString* const kMailFillSymbol = @"envelope.fill";
NSString* const kPhoneFillSymbol = @"phone.fill";
NSString* const kDownloadPromptFillSymbol = @"arrow.down.to.line.circle.fill";
NSString* const kDownloadPromptFilliOS14Symbol = @"arrow.down.circle.fill";
NSString* const kDownloadDocFillSymbol = @"doc.fill";
NSString* const kDocSymbol = @"doc";
NSString* const kOpenInDownloadsSymbol = @"arrow.down.to.line.compact";
NSString* const kOpenInDownloadsiOS14Symbol = @"arrow.down.to.line.alt";
NSString* const kExternalLinkSymbol = @"arrow.up.forward.square";
NSString* const kChevronDownSymbol = @"chevron.down";
NSString* const kChevronForwardSymbol = @"chevron.forward";
NSString* const kGlobeAmericasSymbol = @"globe.americas.fill";
NSString* const kGlobeSymbol = @"globe";
NSString* const kPersonCropCircleSymbol = @"person.crop.circle";
NSString* const kEqualSymbol = @"equal";
NSString* const kBookClosedSymbol = @"book.closed";
NSString* const kSunFillSymbol = @"sun.max.fill";
NSString* const kCalendarSymbol = @"calendar";
NSString* const kTabsSymbol = @"square.on.square";
NSString* const kHighlighterSymbol = @"highlighter";
NSString* const kSealFillSymbol = @"seal.fill";
NSString* const kSquareOnSquareDashedSymbol = @"square.on.square.dashed";
NSString* const kDocPlaintext = @"doc.plaintext";
NSString* const kFlagSymbol = @"flag";
NSString* const kKeyboardSymbol = @"keyboard";
NSString* const kSpeedometerSymbol = @"speedometer";
NSString* const kMovePlatterToTopPhoneSymbol =
    @"platter.filled.top.and.arrow.up.iphone";
NSString* const kMovePlatterToBottomPhoneSymbol =
    @"platter.filled.bottom.and.arrow.down.iphone";
NSString* const kMapSymbol = @"map";

// Names of the default symbol being non-monochrome by default. When using them,
// you probably want to set their color to monochrome.
NSString* const kIPhoneSymbol = @"iphone";
NSString* const kIPadSymbol = @"ipad";
NSString* const kLaptopSymbol = @"laptopcomputer";
NSString* const kDesktopSymbol = @"desktopcomputer";
