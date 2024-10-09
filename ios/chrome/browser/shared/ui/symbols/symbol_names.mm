// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/symbol_names.h"

// Branded symbol names.
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
// TODO(crbug.com/1489185): Move PNG images out of this file.
NSString* const kChromeDefaultBrowserIllustrationImage =
    @"chrome_default_browser_illustration";
NSString* const kChromeDefaultBrowserScreenBannerImage =
    @"chrome_default_browser_screen_banner";
NSString* const kChromeNotificationsOptInBannerImage =
    @"chrome_notifications_opt_in_banner";
NSString* const kChromeNotificationsOptInBannerLandscapeImage =
    @"chrome_notifications_opt_in_banner_landscape";
NSString* const kChromeSearchEngineChoiceIcon =
    @"chrome_search_engine_choice_icon";
NSString* const kChromeSigninBannerImage = @"chrome_signin_banner";
NSString* const kChromeSigninPromoLogoImage = @"chrome_signin_promo_logo";
NSString* const kGoogleDriveSymbol = @"google_drive";
NSString* const kGoogleFullSymbol = @"google_full";
NSString* const kGoogleIconSymbol = @"google_icon";
NSString* const kGoogleShieldSymbol = @"google_shield";
NSString* const kGoogleMapsSymbol = @"google_maps";
NSString* const kGooglePasswordManagerWidgetPromoImage =
    @"google_password_manager_widget_promo";
NSString* const kGooglePasswordManagerWidgetPromoDisabledImage =
    @"google_password_manager_widget_promo_disabled";
NSString* const kGooglePaySymbol = @"google_pay";
NSString* const kGooglePhotosSymbol = @"google_photos";
NSString* const kGooglePlusAddressSymbol = @"google_plus_address";
NSString* const kGoogleSettingsPasswordsInOtherAppsBannerImage =
    @"google_settings_passwords_in_other_apps_banner";
NSString* const kMulticolorChromeballSymbol = @"multicolor_chromeball";
NSString* const kPageInsightsSymbol = @"page_insights";
// TODO(crbug.com/40934931): Move PNG images out of this file.
NSString* const kFedexCarrierImage = @"parcel_tracking_carrier_fedex";
NSString* const kUPSCarrierImage = @"parcel_tracking_carrier_ups";
NSString* const kUSPSCarrierImage = @"parcel_tracking_carrier_usps";
#else
NSString* const kChromiumDefaultBrowserScreenBannerImage =
    @"chromium_default_browser_screen_banner";
NSString* const kChromiumNotificationsOptInBannerImage =
    @"chromium_notifications_opt_in_banner";
NSString* const kChromiumNotificationsOptInBannerLandscapeImage =
    @"chromium_notifications_opt_in_banner_landscape";
NSString* const kChromiumPasswordManagerWidgetPromoImage =
    @"chromium_password_manager_widget_promo";
NSString* const kChromiumPasswordManagerWidgetPromoDisabledImage =
    @"chromium_password_manager_widget_promo_disabled";
NSString* const kChromiumSearchEngineChoiceIcon =
    @"chromium_search_engine_choice_icon";
NSString* const kChromiumSettingsPasswordsInOtherAppsBannerImage =
    @"chromium_settings_passwords_in_other_apps_banner";
NSString* const kChromiumSigninBannerImage = @"chromium_signin_banner";
NSString* const kChromiumSigninPromoLogoImage = @"chromium_signin_promo_logo";
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

// Custom symbol names.
NSString* const kPrivacySymbol = @"checkerboard_shield";
NSString* const kSyncDisabledSymbol = @"arrow_triangle_slash_circlepath";
NSString* const kSafetyCheckSymbol = @"checkermark_shield";
NSString* const kArrowClockWiseSymbol = @"arrow_clockwise";
NSString* const kIncognitoSymbol = @"incognito";
NSString* const kSquareNumberSymbol = @"square_number";
NSString* const kTranslateSymbol = @"translate";
NSString* const kEnterpriseSymbol = @"enterprise";
NSString* const kPasswordManagerSymbol = @"password_manager";
NSString* const kPopupBadgeMinusSymbol = @"popup_badge_minus";
NSString* const kPhotoSymbol = @"photo";
NSString* const kPhotoBadgeArrowDownSymbol = @"photo.badge.arrow.down";
NSString* const kPhotoBadgePlusSymbol = @"photo_badge_plus";
NSString* const kPhotoBadgeMagnifyingglassSymbol =
    @"photo_badge_magnifyingglass";
NSString* const kReadingListSymbol = @"square_bullet_square";
NSString* const kRecentTabsSymbol = @"laptopcomputer_and_phone";
NSString* const kTabGroupsSymbol = @"square.grid.2x2";
NSString* const kLanguageSymbol = @"language";
NSString* const kLocationSymbol = @"location";
NSString* const kPasswordSymbol = @"password";
#if !BUILDFLAG(IS_IOS_MACCATALYST)
NSString* const kMulticolorPasswordSymbol = @"multicolor_password";
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
NSString* const kVoiceSymbol = @"voice";
NSString* const kCameraLensSymbol = @"camera_lens";
NSString* const kDownTrendSymbol = @"line_downtrend";
NSString* const kUpTrendSymbol = @"line_uptrend";
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
NSString* const kArrowDownSymbol = @"arrow.down";
NSString* const kArrowUpSymbol = @"arrow.up";
NSString* const kFamilylinkSymbol = @"familylink";
NSString* const kMyDriveSymbol = @"my_drive";
NSString* const kSharedDrivesSymbol = @"shared_drives";
NSString* const kEllipsisSquareFillSymbol = @"ellipsis_square_fill";

// Custom symbol names which can be configured with a color palette.
NSString* const kIncognitoCircleFillSymbol = @"incognito_circle_fill";
NSString* const kPlusCircleFillSymbol = @"plus_circle_fill";

// Custom symbols added for compatibility with iOS 15.0. These symbols are
// available as system symbols on iOS 15.1+.
NSString* const kCustomMovePlatterToBottomPhoneSymbol =
    @"custom_platter_filled_bottom_and_arrow_down_iphone";
NSString* const kCustomMovePlatterToTopPhoneSymbol =
    @"custom_platter_filled_top_and_arrow_up_iphone";

// Use custom symbol for camera because the default video icon in iOS should
// always represent “Apple Facetime”.
NSString* const kCameraSymbol = @"custom_camera";
NSString* const kCameraFillSymbol = @"custom_camera_fill";

// Default symbol names.
NSString* const kChartBarXAxisSymbol = @"chart.bar.xaxis";
NSString* const kCircleSymbol = @"circle";
NSString* const kCircleFillSymbol = @"circle.fill";
NSString* const kSyncEnabledSymbol = @"arrow.triangle.2.circlepath";
NSString* const kDefaultBrowserSymbol = @"app.badge.checkmark";
NSString* const kDefaultBrowseriOS14Symbol = @"app.badge";
NSString* const kDiscoverSymbol = @"flame";
NSString* const kBellSymbol = @"bell";
NSString* const kBellSlashSymbol = @"bell.slash";
NSString* const kBellBadgeSymbol = @"bell.badge";
NSString* const kCachedDataSymbol = @"photo.on.rectangle";
NSString* const kAutofillDataSymbol = @"wand.and.rays";
NSString* const kSecureLocationBarSymbol = @"lock.fill";
NSString* const kNavigateToTabSymbol = @"arrow.right.circle";
NSString* const kRefineQuerySymbol = @"arrow.up.backward";
NSString* const kLinkActionSymbol = @"link";
NSString* const kQRCodeFinderActionSymbol = @"qrcode.viewfinder";
NSString* const kNewTabActionSymbol = @"plus.square";
NSString* const kPlusInCircleSymbol = @"plus.circle";
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
NSString* const kSecureSymbol = @"lock";
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
NSString* const kExpandSymbol = @"arrow.up.left.and.arrow.down.right";
NSString* const kBackSymbol = @"arrow.backward";
NSString* const kForwardSymbol = @"arrow.forward";
NSString* const kPersonFillSymbol = @"person.fill";
NSString* const kMailFillSymbol = @"envelope.fill";
NSString* const kPhoneFillSymbol = @"phone.fill";
NSString* const kDownloadPromptFillSymbol = @"arrow.down.to.line.circle.fill";
NSString* const kDownloadDocFillSymbol = @"doc.fill";
NSString* const kDocSymbol = @"doc";
NSString* const kOpenInDownloadsSymbol = @"arrow.down.to.line.compact";
NSString* const kExternalLinkSymbol = @"arrow.up.forward.square";
NSString* const kChevronDownSymbol = @"chevron.down";
NSString* const kChevronUpSymbol = @"chevron.up";
NSString* const kChevronBackwardSymbol = @"chevron.backward";
NSString* const kChevronForwardSymbol = @"chevron.forward";
NSString* const kChevronUpDown = @"chevron.up.chevron.down";
NSString* const kChevronDownCircleFill = @"chevron.down.circle.fill";
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
NSString* const kDocPlaintextSymbol = @"doc.plaintext";
NSString* const kFlagSymbol = @"flag";
NSString* const kKeyboardSymbol = @"keyboard";
NSString* const kKeyboardDownSymbol = @"keyboard.chevron.compact.down";
NSString* const kSpeedometerSymbol = @"speedometer";
NSString* const kMovePlatterToTopPhoneSymbol =
    @"platter.filled.top.and.arrow.up.iphone";
NSString* const kMovePlatterToBottomPhoneSymbol =
    @"platter.filled.bottom.and.arrow.down.iphone";
NSString* const kMapSymbol = @"map";
NSString* const kShippingBoxSymbol = @"shippingbox";
NSString* const kSliderHorizontalSymbol = @"slider.horizontal.3";
NSString* const kMacbookAndIPhoneSymbol = @"macbook.and.iphone";
NSString* const kCheckmarkShieldSymbol = @"checkmark.shield";
NSString* const kListBulletClipboardSymbol = @"list.bullet.clipboard";
NSString* const kListBulletRectangleSymbol = @"list.bullet.rectangle.portrait";
NSString* const kBoxTruckFillSymbol = @"box.truck.fill";
NSString* const kExclamationMarkBubbleSymbol = @"exclamationmark.bubble";
NSString* const kShippingBoxFillSymbol = @"shippingbox.fill";
NSString* const kButtonProgrammableSymbol = @"button.programmable";
NSString* const kCircleCircleFillSymbol = @"circle.circle.fill";
NSString* const kLockSymbol = @"lock";
NSString* const kRulerSymbol = @"ruler";
NSString* const kLaptopAndIphoneSymbol = @"laptopcomputer.and.iphone";
NSString* const kBoltSymbol = @"bolt";
NSString* const kLightBulbSymbol = @"lightbulb";
NSString* const kNewTabGroupActionSymbol = @"plus.square.on.square";
NSString* const kRemoveTabFromGroupActionSymbol = @"minus.square";
NSString* const kMoveTabToGroupActionSymbol = @"arrow.up.right.square";
NSString* const kClockSymbol = @"clock";
NSString* const kUngroupTabGroupSymbol = @"viewfinder";
NSString* const kPlusInSquareSymbol = @"plus.square";
NSString* const kMinusInCircleSymbol = @"minus.circle";
NSString* const kMultiIdentitySymbol = @"person.2.fill";
NSString* const kStarBubbleFillSymbol = @"star.bubble.fill";
NSString* const kTurnUpRightDiamondFillSymbol =
    @"arrow.triangle.turn.up.right.diamond.fill";
NSString* const kPencilSymbol = @"pencil";
NSString* const kMagicStackSymbol = @"wand.and.stars.inverse";
NSString* const kDiscoverFeedSymbol = @"newspaper";
NSString* const kFilterSymbol = @"line.3.horizontal.decrease.circle";
NSString* const kSelectedFilterSymbol =
    @"line.3.horizontal.decrease.circle.fill";
NSString* const kPersonTwoSymbol = @"person.2";
NSString* const kSquareFilledOnSquareSymbol = @"square.filled.on.square";
NSString* const kPauseButton = @"pause.circle";
NSString* const kPlayButton = @"play.circle";
NSString* const kFolderSymbol = @"folder";

// Names of the default symbol being non-monochrome by default. When using them,
// you probably want to set their color to monochrome.
NSString* const kIPhoneSymbol = @"iphone";
NSString* const kIPadSymbol = @"ipad";
NSString* const kLaptopSymbol = @"laptopcomputer";
NSString* const kDesktopSymbol = @"desktopcomputer";
