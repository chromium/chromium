// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/symbol_names.h"

// ****************************************************************************
// Branded symbol names.
// ****************************************************************************
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
NSString* const kGeminiFullSymbol = @"gemini_full";
NSString* const kGeminiBrandedLogoSymbol = @"gemini_logo";
NSString* const kGoogleDriveSymbol = @"google_drive";
NSString* const kGoogleFullSymbol = @"google_full";
NSString* const kGoogleIconSymbol = @"google_icon";
NSString* const kGoogleShieldSymbol = @"google_shield";
NSString* const kGoogleMapsSymbol = @"google_maps";
NSString* const kGooglePaySymbol = @"google_pay";
NSString* const kGoogleWalletSymbol = @"google_wallet";
NSString* const kGooglePhotosSymbol = @"google_photos";
NSString* const kMulticolorChromeballSymbol = @"multicolor_chromeball";
NSString* const kPageInsightsSymbol = @"page_insights";
#else
NSString* const kGeminiNonBrandedLogoSymbol = @"sparkle";
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)

// ****************************************************************************
// Custom symbol names.
// ****************************************************************************
NSString* const kPrivacySymbol = @"checkerboard_shield";
NSString* const kSafetyCheckSymbol = @"checkermark_shield";
NSString* const kArrowClockWiseSymbol = @"arrow_clockwise";
NSString* const kIncognitoSymbol = @"incognito";
NSString* const kSquareNumberSymbol = @"square_number";
NSString* const kTranslateSymbol = @"translate";
NSString* const kEnterpriseSigninBannerSymbol = @"enterprise_signin_banner";
NSString* const kEnterpriseSymbol = @"enterprise";
NSString* const kPasswordManagerSymbol = @"password_manager";
NSString* const kPopupBadgeMinusSymbol = @"popup_badge_minus";
NSString* const kPhotoBadgePlusSymbol = @"photo_badge_plus";
NSString* const kPhotoBadgeMagnifyingglassSymbol =
    @"photo_badge_magnifyingglass";
NSString* const kLocationSymbol = @"location";
NSString* const kShieldSymbol = @"shield";
NSString* const kReadingListSymbol = @"square_bullet_square";
NSString* const kRecentTabsSymbol = @"laptopcomputer_and_phone";
NSString* const kLanguageSymbol = @"language";
NSString* const kPassportSymbol = @"passport";
NSString* const kPasswordSymbol = @"password";
#if !BUILDFLAG(IS_IOS_MACCATALYST)
NSString* const kMulticolorPasswordSymbol = @"multicolor_password";
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
NSString* const kVoiceSymbol = @"voice";
NSString* const kCameraLensSymbol = @"camera_lens";
NSString* const kDownTrendSymbol = @"line_downtrend";
NSString* const kUpTrendSymbol = @"line_uptrend";
NSString* const kCloudSlashSymbol = @"cloud_slash";
NSString* const kCloudAndArrowUpSymbol = @"cloud_and_arrow_up";
NSString* const kDinoSymbol = @"dino";
NSString* const kChromeProductSymbol = @"chrome_product";
NSString* const kTunerSymbol = @"tuner";
NSString* const kMoveFolderSymbol = @"folder_badge_arrow_forward";
NSString* const kTopOmniboxOptionSymbol = @"top_omnibox_option";
NSString* const kBottomOmniboxOptionSymbol = @"bottom_omnibox_option";
NSString* const kDangerousOmniboxSymbol = @"dangerous_omnibox";
NSString* const kFamilylinkSymbol = @"familylink";
NSString* const kMyDriveSymbol = @"my_drive";
NSString* const kSharedDrivesSymbol = @"shared_drives";
NSString* const kEllipsisSquareFillSymbol = @"ellipsis_square_fill";
NSString* const kMagnifyingglassSparkSymbol = @"magnifyingglass_spark";
NSString* const kPhoneSparkleSymbol = @"phone_sparkle";
NSString* const kTextSearchSymbol = @"text_search";
NSString* const kIncognitoRectangle = @"incognito_rectangle";
NSString* const kTextAnalysisSymbol = @"text_analysis";
NSString* const kTextSparkSymbol = @"text_spark";
NSString* const kIncognitoCircleFillSymbol = @"incognito_circle_fill";
NSString* const kPlusCircleFillSymbol = @"plus_circle_fill";
NSString* const kPDFFillSymbol = @"pdf_fill";
NSString* const kLineThreeSparkSymbol = @"line_three_spark";
NSString* const kDocumentBadgeSpark = @"document_badge_spark";
NSString* const kDeepSearchSymbol = @"deep_search";

// Use custom symbol for camera because the default video icon in iOS should
// always represent “Apple Facetime”.
NSString* const kCameraSymbol = @"custom_camera";
NSString* const kCameraFillSymbol = @"custom_camera_fill";

// ****************************************************************************
// Default symbol names.
// ****************************************************************************
NSString* const kAppSymbol = @"app";
NSString* const kAppFillSymbol = @"app.fill";
NSString* const kChartBarXAxisSymbol = @"chart.bar.xaxis";
NSString* const kChartLineDowntrendXYAxisSymbol =
    @"chart.line.downtrend.xyaxis";
NSString* const kCircleSymbol = @"circle";
NSString* const kCircleFillSymbol = @"circle.fill";
NSString* const kPhotoSymbol = @"photo";
NSString* const kPhotoBadgeArrowDownSymbol = @"photo.badge.arrow.down";
NSString* const kTabGroupsSymbol = @"square.grid.2x2";
NSString* const kCropSymbol = @"crop";
NSString* const kArrowDownSymbol = @"arrow.down";
NSString* const kArrowUpSymbol = @"arrow.up";
NSString* const kGearshape2Symbol = @"gearshape.2";
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
NSString* const kRefineQueryDownSymbol = @"arrow.down.backward";
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
NSString* const kStarLeadingHalfFilledSymbol = @"star.leadinghalf.filled";
NSString* const kCopyActionSymbol = @"doc.on.doc";
NSString* const kPasteActionSymbol = @"doc.on.clipboard";
NSString* const kPlusRectangleSymbol = @"plus.rectangle";
NSString* const kNewWindowActionSymbol = @"square.split.2x1";
NSString* const kEyedropperSymbol = @"eyedropper";
NSString* const kShowActionSymbol = @"eye";
NSString* const kHideActionSymbol = @"eye.slash";
NSString* const kFindInPageActionSymbol = @"doc.text.magnifyingglass";
NSString* const kZoomTextActionSymbol = @"plus.magnifyingglass";
NSString* const kSaveImageActionSymbol = @"square.and.arrow.down";
NSString* const kOpenImageActionSymbol = @"arrow.up.right.square";
NSString* const kQRCodeSymbol = @"qrcode";
NSString* const kPrinterSymbol = @"printer";
NSString* const kAirplaneSymbol = @"airplane";
NSString* const kAirplaneUpRightSymbol = @"airplane.up.right";
NSString* const kCarSymbol = @"car";
NSString* const kCreditCardSymbol = @"creditcard";
NSString* const kMicrophoneFillSymbol = @"mic.fill";
NSString* const kMicrophoneSymbol = @"mic";
NSString* const kMagnifyingglassSymbol = @"magnifyingglass";
NSString* const kMagnifyingglassCircleSymbol = @"magnifyingglass.circle";
NSString* const kEllipsisCircleFillSymbol = @"ellipsis.circle.fill";
NSString* const kEllipsisRectangleSymbol = @"ellipsis.rectangle";
NSString* const kEllipsisSymbol = @"ellipsis";
NSString* const kPinSymbol = @"pin";
NSString* const kPinSlashSymbol = @"pin.slash";
NSString* const kSettingsSymbol = @"gearshape";
NSString* const kSettingsFilledSymbol = @"gearshape.fill";
NSString* const kShareSymbol = @"square.and.arrow.up";
NSString* const kXMarkSymbol = @"xmark";
NSString* const kXMarkSquareSymbol = @"xmark.square";
NSString* const kXMarkSquareFillSymbol = @"xmark.square.fill";
NSString* const kXMarkCircleSymbol = @"xmark.circle";
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
NSString* const kSyncPasswordErrorSymbol =
    @"lock.trianglebadge.exclamationmark.fill";
NSString* const kMenuSymbol = @"ellipsis";
NSString* const kSortSymbol = @"arrow.up.arrow.down";
NSString* const kExpandSymbol = @"arrow.up.left.and.arrow.down.right";
NSString* const kBackSymbol = @"arrow.backward";
NSString* const kForwardSymbol = @"arrow.forward";
NSString* const kPersonFillSymbol = @"person.fill";
NSString* const kPersonFillCheckmarkSymbol = @"person.fill.checkmark";
NSString* const kPersonTextRectangleSymbol = @"person.text.rectangle";
NSString* const kPersonBadgeKeyFillSymbol = @"person.badge.key.fill";
NSString* const kPersonClockFillSymbol = @"person.badge.clock.fill";
NSString* const kPersonFillBadgePlusSymbol = @"person.fill.badge.plus";
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
NSString* const kChevronRightSymbol = @"chevron.right";
NSString* const kChevronUpDown = @"chevron.up.chevron.down";
NSString* const kChevronDownCircleFill = @"chevron.down.circle.fill";
NSString* const kGlobeAmericasSymbol = @"globe.americas.fill";
NSString* const kGlobeSymbol = @"globe";
NSString* const kPersonCropCircleSymbol = @"person.crop.circle";
NSString* const kEqualSymbol = @"equal";
NSString* const kBookClosedSymbol = @"book.closed";
NSString* const kSunFillSymbol = @"sun.max.fill";
NSString* const kCalendarSymbol = @"calendar";
NSString* const kArrowLeftSymbol = @"arrow.left";
NSString* const kArrowRightSymbol = @"arrow.right";
NSString* const kArrowLeftSquareSymbol = @"arrow.left.square";
NSString* const kArrowRightSquareSymbol = @"arrow.right.square";
NSString* const kArrowLeftToLineSquareSymbol = @"arrow.left.to.line.square";
NSString* const kArrowRightToLineSquareSymbol = @"arrow.right.to.line.square";
NSString* const kClockArrowTriangleheadCounterclockwiseRotate90Symbol =
    @"clock.arrow.trianglehead.counterclockwise.rotate.90";
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
NSString* const kSliderHorizontalSymbol = @"slider.horizontal.3";
NSString* const kMacbookAndIPhoneSymbol = @"macbook.and.iphone";
NSString* const kCheckmarkShieldSymbol = @"checkmark.shield";
NSString* const kListBulletSymbol = @"list.bullet";
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
NSString* const kFolderBadgePlusSymbol = @"folder.badge.plus";
NSString* const kCartSymbol = @"cart";
NSString* const kArrowUTurnForwardSymbol = @"arrow.uturn.forward";
NSString* const kArrowUTurnForwardCircleFillSymbol =
    @"arrow.uturn.forward.circle.fill";
NSString* const kArrowUTurnBackwardSymbol = @"arrow.uturn.backward";
NSString* const kIPhoneAndArrowForwardSymbol = @"iphone.and.arrow.forward";
NSString* const kPersonPlusSymbol = @"person.crop.circle.badge.plus";
NSString* const kArrowUpTrashSymbol = @"arrow.up.trash";
NSString* const kRectangleGroupBubble = @"rectangle.3.group.bubble";
NSString* const kHomeSymbol = @"house";
NSString* const kWorkSymbol = @"case";
NSString* const kShieldedEnvelope = @"envelope.badge.shield.half.filled";
NSString* const kReaderModeSymbolPreIOS18 = @"doc.plaintext";
NSString* const kReaderModeSymbolPostIOS18 = @"text.page";
NSString* const kCircleBadgeFill = @"circlebadge.fill";
NSString* const kCounterClockWiseSymbol =
    @"clock.arrow.trianglehead.counterclockwise.rotate.90";
NSString* const kBuilding2Symbol = @"building.2";
NSString* const kBookSymbol = @"book";
NSString* const kKeySymbol = @"key";
NSString* const kTextDocument = @"text.document";
NSString* const kTextJustifyLeftSymbol = @"text.justifyleft";
NSString* const kVideoSymbol = @"video";
NSString* const kWaveformSymbol = @"waveform.mid";
NSString* const kPhotoOnRectangleSymbol = @"photo.on.rectangle";
NSString* const kSystemCameraSymbol = @"camera";
NSString* const kRightArrowCircleFillSymbol = @"arrow.right.circle.fill";
NSString* const kArrowDownToLineSymbol = @"arrow.down.to.line";
NSString* const kPhotoOnRectangleAngled = @"photo.on.rectangle.angled";
NSString* const kSparklesSymbol = @"sparkles";
NSString* const kSparkles2Symbol = @"sparkles.2";

// Names of the default symbol being non-monochrome by default. When using them,
// you probably want to set their color to monochrome.
NSString* const kIPhoneSymbol = @"iphone";
NSString* const kIPadSymbol = @"ipad";
NSString* const kLaptopSymbol = @"laptopcomputer";
NSString* const kDesktopSymbol = @"desktopcomputer";
