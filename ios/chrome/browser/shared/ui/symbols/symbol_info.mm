// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/symbol_info.h"

#import "base/not_fatal_until.h"
#import "base/notreached.h"

SymbolInfo InfoForSymbol(Symbol symbol) {
  switch (symbol) {
    case SymbolNone:
      NOTREACHED(base::NotFatalUntil::M160);
      return {nil, SymbolType::kSystem};

      // Branded symbols.
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
    case SymbolGeminiBrandedLogo:
      return {@"gemini_logo", SymbolType::kCustom};
    case SymbolGeminiFull:
      return {@"gemini_full", SymbolType::kCustom};
    case SymbolGeminiLiveLogo:
      return {@"gemini_live", SymbolType::kCustom};
    case SymbolGoogleDrive:
      return {@"google_drive", SymbolType::kCustom};
    case SymbolGoogleFull:
      return {@"google_full", SymbolType::kCustom};
    case SymbolGoogleIcon:
      return {@"google_icon", SymbolType::kCustom};
    case SymbolGoogleMaps:
      return {@"google_maps", SymbolType::kCustom};
    case SymbolGooglePay:
      return {@"google_pay", SymbolType::kCustom};
    case SymbolGooglePayV2:
      return {@"google_pay_v2", SymbolType::kCustom};
    case SymbolGooglePhotos:
      return {@"google_photos", SymbolType::kCustom};
    case SymbolGoogleShield:
      return {@"google_shield", SymbolType::kCustom};
    case SymbolGoogleWallet:
      return {@"google_wallet", SymbolType::kCustom};
    case SymbolGoogleWalletIcon:
      return {@"google_wallet_icon", SymbolType::kCustom};
    case SymbolGoogleWalletIconV2:
      return {@"google_wallet_icon_v2", SymbolType::kCustom};
    case SymbolGoogleWalletV2:
      return {@"google_wallet_v2", SymbolType::kCustom};
    case SymbolGPayPillIcon:
      return {@"gpay_pill_icon", SymbolType::kCustom};
    case SymbolGPayPillIconV2:
      return {@"gpay_pill_icon_v2", SymbolType::kCustom};
    case SymbolMulticolorChromeball:
      return {@"multicolor_chromeball", SymbolType::kCustom};
    case SymbolPageInsights:
      return {@"page_insights", SymbolType::kCustom};
#else
    case SymbolGeminiNonBrandedLogo:
      return {@"sparkle", SymbolType::kSystem};
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)

      // Custom symbols.
    case SymbolAirplaneUp:
      return {@"airplane_up", SymbolType::kCustom};
    case SymbolAirplaneUpSpark:
      return {@"airplane_up_spark", SymbolType::kCustom};
    case SymbolArrowClockWise:
      return {@"arrow_clockwise", SymbolType::kCustom};
    case SymbolBagSpark:
      return {@"bag_spark", SymbolType::kCustom};
    case SymbolBottomOmniboxOption:
      return {@"bottom_omnibox_option", SymbolType::kCustom};
    case SymbolCamera:
      return {@"custom_camera", SymbolType::kCustom};
    case SymbolCameraFill:
      return {@"custom_camera_fill", SymbolType::kCustom};
    case SymbolCameraLens:
      return {@"camera_lens", SymbolType::kCustom};
    case SymbolCarSpark:
      return {@"car_spark", SymbolType::kCustom};
    case SymbolChromeProduct:
      return {@"chrome_product", SymbolType::kCustom};
    case SymbolCloudAndArrowUp:
      return {@"cloud_and_arrow_up", SymbolType::kCustom};
    case SymbolCloudSlash:
      return {@"cloud_slash", SymbolType::kCustom};
    case SymbolDangerousOmnibox:
      return {@"dangerous_omnibox", SymbolType::kCustom};
    case SymbolDeepSearch:
      return {@"deep_search", SymbolType::kCustom};
    case SymbolDino:
      return {@"dino", SymbolType::kCustom};
    case SymbolDocumentBadgeSpark:
      return {@"document_badge_spark", SymbolType::kCustom};
    case SymbolDownTrend:
      return {@"line_downtrend", SymbolType::kCustom};
    case SymbolEllipsisSquareFill:
      return {@"ellipsis_square_fill", SymbolType::kCustom};
    case SymbolEnterprise:
      return {@"enterprise", SymbolType::kCustom};
    case SymbolEnterpriseSigninBanner:
      return {@"enterprise_signin_banner", SymbolType::kCustom};
    case SymbolFamilylink:
      return {@"familylink", SymbolType::kCustom};
    case SymbolIncognito:
      return {@"incognito", SymbolType::kCustom};
    case SymbolIncognitoCircleFill:
      return {@"incognito_circle_fill", SymbolType::kCustom};
    case SymbolIncognitoRectangle:
      return {@"incognito_rectangle", SymbolType::kCustom};
    case SymbolLanguage:
      return {@"language", SymbolType::kCustom};
    case SymbolLineThreeSpark:
      return {@"line_three_spark", SymbolType::kCustom};
    case SymbolLocation:
      return {@"location", SymbolType::kCustom};
    case SymbolLocationSpark:
      return {@"location_spark", SymbolType::kCustom};
    case SymbolMagnifyingglassSpark:
      return {@"magnifyingglass_spark", SymbolType::kCustom};
    case SymbolMoveFolder:
      return {@"folder_badge_arrow_forward", SymbolType::kCustom};
#if !BUILDFLAG(IS_IOS_MACCATALYST)
    case SymbolMulticolorPassword:
      return {@"multicolor_password", SymbolType::kCustom};
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
    case SymbolMyDrive:
      return {@"my_drive", SymbolType::kCustom};
    case SymbolPDFFill:
      return {@"pdf_fill", SymbolType::kCustom};
    case SymbolPassport:
      return {@"passport", SymbolType::kCustom};
    case SymbolPassportSpark:
      return {@"passport_spark", SymbolType::kCustom};
    case SymbolPassword:
      return {@"password", SymbolType::kCustom};
    case SymbolPasswordManager:
      return {@"password_manager", SymbolType::kCustom};
    case SymbolPersonTextRectangle2:
      return {@"person_text_rectangle_2", SymbolType::kCustom};
    case SymbolPersonTextRectangle2Spark:
      return {@"person_text_rectangle_2_spark", SymbolType::kCustom};
    case SymbolPersonTextRectangleSpark:
      return {@"person_text_rectangle_spark", SymbolType::kCustom};
    case SymbolPhoneSparkle:
      return {@"phone_sparkle", SymbolType::kCustom};
    case SymbolPhotoBadgeMagnifyingglass:
      return {@"photo_badge_magnifyingglass", SymbolType::kCustom};
    case SymbolPhotoBadgePlus:
      return {@"photo_badge_plus", SymbolType::kCustom};
    case SymbolPlusCircleFill:
      return {@"plus_circle_fill", SymbolType::kCustom};
    case SymbolPopupBadgeMinus:
      return {@"popup_badge_minus", SymbolType::kCustom};
    case SymbolPrivacy:
      return {@"checkerboard_shield", SymbolType::kCustom};
    case SymbolReadingList:
      return {@"square_bullet_square", SymbolType::kCustom};
    case SymbolRecentTabs:
      return {@"laptopcomputer_and_phone", SymbolType::kCustom};
    case SymbolSafetyCheck:
      return {@"checkermark_shield", SymbolType::kCustom};
    case SymbolSharedDrives:
      return {@"shared_drives", SymbolType::kCustom};
    case SymbolShield:
      return {@"shield", SymbolType::kCustom};
    case SymbolSquareNumber:
      return {@"square_number", SymbolType::kCustom};
    case SymbolTextAnalysis:
      return {@"text_analysis", SymbolType::kCustom};
    case SymbolTextSearch:
      return {@"text_search", SymbolType::kCustom};
    case SymbolTextSpark:
      return {@"text_spark", SymbolType::kCustom};
    case SymbolTopOmniboxOption:
      return {@"top_omnibox_option", SymbolType::kCustom};
    case SymbolTranslate:
      return {@"translate", SymbolType::kCustom};
    case SymbolTruckBoxSpark:
      return {@"truck_box_spark", SymbolType::kCustom};
    case SymbolTuner:
      return {@"tuner", SymbolType::kCustom};
    case SymbolUpTrend:
      return {@"line_uptrend", SymbolType::kCustom};
    case SymbolVoice:
      return {@"voice", SymbolType::kCustom};

      // System symbols.
    case SymbolAddBookmarkAction:
      return {@"star", SymbolType::kSystem};
    case SymbolApp:
      return {@"app", SymbolType::kSystem};
    case SymbolAppFill:
      return {@"app.fill", SymbolType::kSystem};
    case SymbolArrowDown:
      return {@"arrow.down", SymbolType::kSystem};
    case SymbolArrowDownCircleFill:
      return {@"arrow.down.circle.fill", SymbolType::kSystem};
    case SymbolArrowDownToLine:
      return {@"arrow.down.to.line", SymbolType::kSystem};
    case SymbolArrowLeft:
      return {@"arrow.left", SymbolType::kSystem};
    case SymbolArrowLeftSquare:
      return {@"arrow.left.square", SymbolType::kSystem};
    case SymbolArrowLeftToLineSquare:
      return {@"arrow.left.to.line.square", SymbolType::kSystem};
    case SymbolArrowRight:
      return {@"arrow.right", SymbolType::kSystem};
    case SymbolArrowRightSquare:
      return {@"arrow.right.square", SymbolType::kSystem};
    case SymbolArrowRightToLineSquare:
      return {@"arrow.right.to.line.square", SymbolType::kSystem};
    case SymbolArrowTrianglehead2ClockwiseRotate90:
      return {@"arrow.trianglehead.2.clockwise.rotate.90", SymbolType::kSystem};
    case SymbolArrowUTurnBackward:
      return {@"arrow.uturn.backward", SymbolType::kSystem};
    case SymbolArrowUTurnForward:
      return {@"arrow.uturn.forward", SymbolType::kSystem};
    case SymbolArrowUTurnForwardCircleFill:
      return {@"arrow.uturn.forward.circle.fill", SymbolType::kSystem};
    case SymbolArrowUp:
      return {@"arrow.up", SymbolType::kSystem};
    case SymbolArrowUpCircleFill:
      return {@"arrow.up.circle.fill", SymbolType::kSystem};
    case SymbolArrowUpTrash:
      return {@"arrow.up.trash", SymbolType::kSystem};
    case SymbolAutofillData:
      return {@"wand.and.rays", SymbolType::kSystem};
    case SymbolBack:
      return {@"arrow.backward", SymbolType::kSystem};
    case SymbolBag:
      return {@"bag", SymbolType::kSystem};
    case SymbolBell:
      return {@"bell", SymbolType::kSystem};
    case SymbolBellBadge:
      return {@"bell.badge", SymbolType::kSystem};
    case SymbolBellSlash:
      return {@"bell.slash", SymbolType::kSystem};
    case SymbolBinocularsCircle:
      return {@"binoculars.circle", SymbolType::kSystem};
    case SymbolBolt:
      return {@"bolt", SymbolType::kSystem};
    case SymbolBook:
      return {@"book", SymbolType::kSystem};
    case SymbolBookClosed:
      return {@"book.closed", SymbolType::kSystem};
    case SymbolBookmarks:
      return {@"star", SymbolType::kSystem};
    case SymbolBoxTruckFill:
      return {@"box.truck.fill", SymbolType::kSystem};
    case SymbolBuilding2:
      return {@"building.2", SymbolType::kSystem};
    case SymbolButtonProgrammable:
      return {@"button.programmable", SymbolType::kSystem};
    case SymbolCachedData:
      return {@"photo.on.rectangle", SymbolType::kSystem};
    case SymbolCalendar:
      return {@"calendar", SymbolType::kSystem};
    case SymbolCar:
      return {@"car", SymbolType::kSystem};
    case SymbolCart:
      return {@"cart", SymbolType::kSystem};
    case SymbolChartBarXAxis:
      return {@"chart.bar.xaxis", SymbolType::kSystem};
    case SymbolChartLineDowntrendXYAxis:
      return {@"chart.line.downtrend.xyaxis", SymbolType::kSystem};
    case SymbolCheckmark:
      return {@"checkmark", SymbolType::kSystem};
    case SymbolCheckmarkCircle:
      return {@"checkmark.circle", SymbolType::kSystem};
    case SymbolCheckmarkCircleFill:
      return {@"checkmark.circle.fill", SymbolType::kSystem};
    case SymbolCheckmarkSeal:
      return {@"checkmark.seal", SymbolType::kSystem};
    case SymbolCheckmarkSealFill:
      return {@"checkmark.seal.fill", SymbolType::kSystem};
    case SymbolCheckmarkShield:
      return {@"checkmark.shield", SymbolType::kSystem};
    case SymbolChevronBackward:
      return {@"chevron.backward", SymbolType::kSystem};
    case SymbolChevronDown:
      return {@"chevron.down", SymbolType::kSystem};
    case SymbolChevronDownCircleFill:
      return {@"chevron.down.circle.fill", SymbolType::kSystem};
    case SymbolChevronForward:
      return {@"chevron.forward", SymbolType::kSystem};
    case SymbolChevronRight:
      return {@"chevron.right", SymbolType::kSystem};
    case SymbolChevronUp:
      return {@"chevron.up", SymbolType::kSystem};
    case SymbolChevronUpDown:
      return {@"chevron.up.chevron.down", SymbolType::kSystem};
    case SymbolCircle:
      return {@"circle", SymbolType::kSystem};
    case SymbolCircleBadgeFill:
      return {@"circlebadge.fill", SymbolType::kSystem};
    case SymbolCircleCircleFill:
      return {@"circle.circle.fill", SymbolType::kSystem};
    case SymbolCircleFill:
      return {@"circle.fill", SymbolType::kSystem};
    case SymbolClipboardAction:
      return {@"doc.on.clipboard", SymbolType::kSystem};
    case SymbolClock:
      return {@"clock", SymbolType::kSystem};
    case SymbolClockArrowTriangleheadCounterclockwiseRotate90:
      return {@"clock.arrow.trianglehead.counterclockwise.rotate.90",
              SymbolType::kSystem};
    case SymbolCopyAction:
      return {@"doc.on.doc", SymbolType::kSystem};
    case SymbolCounterClockWise:
      return {@"clock.arrow.trianglehead.counterclockwise.rotate.90",
              SymbolType::kSystem};
    case SymbolCreditCard:
      return {@"creditcard", SymbolType::kSystem};
    case SymbolCreditCardFinderAction:
      return {@"creditcard.viewfinder", SymbolType::kSystem};
    case SymbolCrop:
      return {@"crop", SymbolType::kSystem};
    case SymbolCursorArrow:
      return {@"cursorarrow", SymbolType::kSystem};
    case SymbolCursorArrowMotionLines:
      return {@"cursorarrow.motionlines", SymbolType::kSystem};
    case SymbolCursorArrowRays:
      return {@"cursorarrow.rays", SymbolType::kSystem};
    case SymbolDefaultBrowser:
      return {@"app.badge.checkmark", SymbolType::kSystem};
    case SymbolDefaultBrowseriOS14:
      return {@"app.badge", SymbolType::kSystem};
    case SymbolDeleteAction:
      return {@"trash", SymbolType::kSystem};
    case SymbolDesktop:
      return {@"desktopcomputer", SymbolType::kSystem};
    case SymbolDiscover:
      return {@"flame", SymbolType::kSystem};
    case SymbolDiscoverFeed:
      return {@"newspaper", SymbolType::kSystem};
    case SymbolDoc:
      return {@"doc", SymbolType::kSystem};
    case SymbolDocPlaintext:
      return {@"doc.plaintext", SymbolType::kSystem};
    case SymbolDownload:
      return {@"arrow.down.circle", SymbolType::kSystem};
    case SymbolDownloadDocFill:
      return {@"doc.fill", SymbolType::kSystem};
    case SymbolDownloadPromptFill:
      return {@"arrow.down.to.line.circle.fill", SymbolType::kSystem};
    case SymbolEditAction:
      return {@"pencil", SymbolType::kSystem};
    case SymbolEllipsis:
      return {@"ellipsis", SymbolType::kSystem};
    case SymbolEllipsisCircleFill:
      return {@"ellipsis.circle.fill", SymbolType::kSystem};
    case SymbolEllipsisRectangle:
      return {@"ellipsis.rectangle", SymbolType::kSystem};
    case SymbolEnvelope:
      return {@"envelope", SymbolType::kSystem};
    case SymbolEqual:
      return {@"equal", SymbolType::kSystem};
    case SymbolErrorCircle:
      return {@"exclamationmark.circle", SymbolType::kSystem};
    case SymbolErrorCircleFill:
      return {@"exclamationmark.circle.fill", SymbolType::kSystem};
    case SymbolExclamationMarkBubble:
      return {@"exclamationmark.bubble", SymbolType::kSystem};
    case SymbolExpand:
      return {@"arrow.up.left.and.arrow.down.right", SymbolType::kSystem};
    case SymbolExternalLink:
      return {@"arrow.up.forward.square", SymbolType::kSystem};
    case SymbolEyedropper:
      return {@"eyedropper", SymbolType::kSystem};
    case SymbolFilter:
      return {@"line.3.horizontal.decrease.circle", SymbolType::kSystem};
    case SymbolFindInPageAction:
      return {@"doc.text.magnifyingglass", SymbolType::kSystem};
    case SymbolFlag:
      return {@"flag", SymbolType::kSystem};
    case SymbolFolder:
      return {@"folder", SymbolType::kSystem};
    case SymbolFolderBadgePlus:
      return {@"folder.badge.plus", SymbolType::kSystem};
    case SymbolForward:
      return {@"arrow.forward", SymbolType::kSystem};
    case SymbolGearshape2:
      return {@"gearshape.2", SymbolType::kSystem};
    case SymbolGlobe:
      return {@"globe", SymbolType::kSystem};
    case SymbolGlobeAmericas:
      return {@"globe.americas.fill", SymbolType::kSystem};
    case SymbolHelp:
      return {@"questionmark.circle", SymbolType::kSystem};
    case SymbolHideAction:
      return {@"eye.slash", SymbolType::kSystem};
    case SymbolHighlighter:
      return {@"highlighter", SymbolType::kSystem};
    case SymbolHistory:
      return {@"clock.arrow.circlepath", SymbolType::kSystem};
    case SymbolHome:
      return {@"house", SymbolType::kSystem};
    case SymbolHourglass:
      return {@"hourglass", SymbolType::kSystem};
    case SymbolIPad:
      return {@"ipad", SymbolType::kSystem};
    case SymbolIPhone:
      return {@"iphone", SymbolType::kSystem};
    case SymbolIPhoneAndArrowForward:
      return {@"iphone.and.arrow.forward", SymbolType::kSystem};
    case SymbolInfoCircle:
      return {@"info.circle", SymbolType::kSystem};
    case SymbolKey:
      return {@"key", SymbolType::kSystem};
    case SymbolKeyboard:
      return {@"keyboard", SymbolType::kSystem};
    case SymbolKeyboardDown:
      return {@"keyboard.chevron.compact.down", SymbolType::kSystem};
    case SymbolLadybugCircleFill:
      return {@"ladybug.circle.fill", SymbolType::kSystem};
    case SymbolLaptop:
      return {@"laptopcomputer", SymbolType::kSystem};
    case SymbolLaptopAndIphone:
      return {@"laptopcomputer.and.iphone", SymbolType::kSystem};
    case SymbolLightBulb:
      return {@"lightbulb", SymbolType::kSystem};
    case SymbolLinkAction:
      return {@"link", SymbolType::kSystem};
    case SymbolListBullet:
      return {@"list.bullet", SymbolType::kSystem};
    case SymbolListBulletClipboard:
      return {@"list.bullet.clipboard", SymbolType::kSystem};
    case SymbolListBulletRectangle:
      return {@"list.bullet.rectangle.portrait", SymbolType::kSystem};
    case SymbolLock:
      return {@"lock", SymbolType::kSystem};
    case SymbolMacbookAndIPhone:
      return {@"macbook.and.iphone", SymbolType::kSystem};
    case SymbolMagicStack:
      return {@"wand.and.stars.inverse", SymbolType::kSystem};
    case SymbolMagnifyingglass:
      return {@"magnifyingglass", SymbolType::kSystem};
    case SymbolMagnifyingglassCircle:
      return {@"magnifyingglass.circle", SymbolType::kSystem};
    case SymbolMailFill:
      return {@"envelope.fill", SymbolType::kSystem};
    case SymbolMap:
      return {@"map", SymbolType::kSystem};
    case SymbolMarkAsReadAction:
      return {@"text.badge.checkmark", SymbolType::kSystem};
    case SymbolMarkAsUnreadAction:
      return {@"text.badge.minus", SymbolType::kSystem};
    case SymbolMenu:
      return {@"ellipsis", SymbolType::kSystem};
    case SymbolMicrophone:
      return {@"mic", SymbolType::kSystem};
    case SymbolMicrophoneFill:
      return {@"mic.fill", SymbolType::kSystem};
    case SymbolMinusInCircle:
      return {@"minus.circle", SymbolType::kSystem};
    case SymbolMovePlatterToBottomPhone:
      return {@"platter.filled.bottom.and.arrow.down.iphone",
              SymbolType::kSystem};
    case SymbolMovePlatterToTopPhone:
      return {@"platter.filled.top.and.arrow.up.iphone", SymbolType::kSystem};
    case SymbolMoveTabToGroupAction:
      return {@"arrow.up.right.square", SymbolType::kSystem};
    case SymbolMultiIdentity:
      return {@"person.2.fill", SymbolType::kSystem};
    case SymbolNavigateToTab:
      return {@"arrow.right.circle", SymbolType::kSystem};
    case SymbolNewTabAction:
      return {@"plus.square", SymbolType::kSystem};
    case SymbolNewTabGroupAction:
      return {@"plus.square.on.square", SymbolType::kSystem};
    case SymbolNewWindowAction:
      return {@"square.split.2x1", SymbolType::kSystem};
    case SymbolOpenImageAction:
      return {@"arrow.up.right.square", SymbolType::kSystem};
    case SymbolOpenInDownloads:
      return {@"arrow.down.to.line.compact", SymbolType::kSystem};
    case SymbolPaperclip:
      return {@"paperclip", SymbolType::kSystem};
    case SymbolPasteAction:
      return {@"doc.on.clipboard", SymbolType::kSystem};
    case SymbolPauseButton:
      return {@"pause.circle", SymbolType::kSystem};
    case SymbolPauseFill:
      return {@"pause.fill", SymbolType::kSystem};
    case SymbolPencil:
      return {@"pencil", SymbolType::kSystem};
    case SymbolPersonBadgeKeyFill:
      return {@"person.badge.key.fill", SymbolType::kSystem};
    case SymbolPersonClockFill:
      return {@"person.badge.clock.fill", SymbolType::kSystem};
    case SymbolPersonCropCircle:
      return {@"person.crop.circle", SymbolType::kSystem};
    case SymbolPersonFill:
      return {@"person.fill", SymbolType::kSystem};
    case SymbolPersonFillBadgePlus:
      return {@"person.fill.badge.plus", SymbolType::kSystem};
    case SymbolPersonPlus:
      return {@"person.crop.circle.badge.plus", SymbolType::kSystem};
    case SymbolPersonTextRectangle:
      return {@"person.text.rectangle", SymbolType::kSystem};
    case SymbolPersonTwo:
      return {@"person.2", SymbolType::kSystem};
    case SymbolPhoneFill:
      return {@"phone.fill", SymbolType::kSystem};
    case SymbolPhoto:
      return {@"photo", SymbolType::kSystem};
    case SymbolPhotoBadgeArrowDown:
      return {@"photo.badge.arrow.down", SymbolType::kSystem};
    case SymbolPhotoOnRectangle:
      return {@"photo.on.rectangle", SymbolType::kSystem};
    case SymbolPhotoOnRectangleAngled:
      return {@"photo.on.rectangle.angled", SymbolType::kSystem};
    case SymbolPin:
      return {@"pin", SymbolType::kSystem};
    case SymbolPinSlash:
      return {@"pin.slash", SymbolType::kSystem};
    case SymbolPlayButton:
      return {@"play.circle", SymbolType::kSystem};
    case SymbolPlayFill:
      return {@"play.fill", SymbolType::kSystem};
    case SymbolPlus:
      return {@"plus", SymbolType::kSystem};
    case SymbolPlusInCircle:
      return {@"plus.circle", SymbolType::kSystem};
    case SymbolPlusInSquare:
      return {@"plus.square", SymbolType::kSystem};
    case SymbolPlusRectangle:
      return {@"plus.rectangle", SymbolType::kSystem};
    case SymbolPrinter:
      return {@"printer", SymbolType::kSystem};
    case SymbolPuzzlePieceExtension:
      return {@"puzzlepiece.extension", SymbolType::kSystem};
    case SymbolQRCode:
      return {@"qrcode", SymbolType::kSystem};
    case SymbolQRCodeFinderAction:
      return {@"qrcode.viewfinder", SymbolType::kSystem};
    case SymbolReadLaterAction:
      return {@"text.badge.plus", SymbolType::kSystem};
    case SymbolReaderMode:
      return {@"text.page", SymbolType::kSystem};
    case SymbolRectangleGroupBubble:
      return {@"rectangle.3.group.bubble", SymbolType::kSystem};
    case SymbolRefineQuery:
      return {@"arrow.up.backward", SymbolType::kSystem};
    case SymbolRefineQueryDown:
      return {@"arrow.down.backward", SymbolType::kSystem};
    case SymbolRemoveTabFromGroupAction:
      return {@"minus.square", SymbolType::kSystem};
    case SymbolRightArrowCircleFill:
      return {@"arrow.right.circle.fill", SymbolType::kSystem};
    case SymbolRuler:
      return {@"ruler", SymbolType::kSystem};
    case SymbolSaveImageAction:
      return {@"square.and.arrow.down", SymbolType::kSystem};
    case SymbolSealFill:
      return {@"seal.fill", SymbolType::kSystem};
    case SymbolSearch:
      return {@"magnifyingglass", SymbolType::kSystem};
    case SymbolSecure:
      return {@"lock", SymbolType::kSystem};
    case SymbolSecureLocationBar:
      return {@"lock.fill", SymbolType::kSystem};
    case SymbolSelectedFilter:
      return {@"line.3.horizontal.decrease.circle.fill", SymbolType::kSystem};
    case SymbolSettings:
      return {@"gearshape", SymbolType::kSystem};
    case SymbolSettingsFilled:
      return {@"gearshape.fill", SymbolType::kSystem};
    case SymbolShare:
      return {@"square.and.arrow.up", SymbolType::kSystem};
    case SymbolShippingBoxFill:
      return {@"shippingbox.fill", SymbolType::kSystem};
    case SymbolShowAction:
      return {@"eye", SymbolType::kSystem};
    case SymbolSliderHorizontal:
      return {@"slider.horizontal.3", SymbolType::kSystem};
    case SymbolSort:
      return {@"arrow.up.arrow.down", SymbolType::kSystem};
    case SymbolSparkles:
      return {@"sparkles", SymbolType::kSystem};
    case SymbolSparkles2:
      return {@"sparkles.2", SymbolType::kSystem};
    case SymbolSpeedometer:
      return {@"speedometer", SymbolType::kSystem};
    case SymbolSquareAndPencil:
      return {@"square.and.pencil", SymbolType::kSystem};
    case SymbolSquareFilledOnSquare:
      return {@"square.filled.on.square", SymbolType::kSystem};
    case SymbolSquareOnSquareDashed:
      return {@"square.on.square.dashed", SymbolType::kSystem};
    case SymbolStarBubbleFill:
      return {@"star.bubble.fill", SymbolType::kSystem};
    case SymbolStarLeadingHalfFilled:
      return {@"star.leadinghalf.filled", SymbolType::kSystem};
    case SymbolSuitcase:
      return {@"suitcase", SymbolType::kSystem};
    case SymbolSunFill:
      return {@"sun.max.fill", SymbolType::kSystem};
    case SymbolSyncEnabled:
      return {@"arrow.triangle.2.circlepath", SymbolType::kSystem};
    case SymbolSyncError:
      return {@"exclamationmark.arrow.triangle.2.circlepath",
              SymbolType::kSystem};
    case SymbolSyncPasswordError:
      return {@"lock.trianglebadge.exclamationmark.fill", SymbolType::kSystem};
    case SymbolSystemCamera:
      return {@"camera", SymbolType::kSystem};
    case SymbolTabGroups:
      return {@"square.grid.2x2", SymbolType::kSystem};
    case SymbolTabs:
      return {@"square.on.square", SymbolType::kSystem};
    case SymbolTextDocument:
      return {@"text.document", SymbolType::kSystem};
    case SymbolTextJustifyLeft:
      return {@"text.justifyleft", SymbolType::kSystem};
    case SymbolTrash:
      return {@"trash", SymbolType::kSystem};
    case SymbolTruckBox:
      return {@"truck.box", SymbolType::kSystem};
    case SymbolTurnUpRightDiamondFill:
      return {@"arrow.triangle.turn.up.right.diamond.fill",
              SymbolType::kSystem};
    case SymbolUngroupTabGroup:
      return {@"viewfinder", SymbolType::kSystem};
    case SymbolVideo:
      return {@"video", SymbolType::kSystem};
    case SymbolWalletBifold:
      return {@"wallet.bifold", SymbolType::kSystem};
    case SymbolWarning:
      return {@"exclamationmark.triangle", SymbolType::kSystem};
    case SymbolWarningFill:
      return {@"exclamationmark.triangle.fill", SymbolType::kSystem};
    case SymbolWaveform:
      return {@"waveform.mid", SymbolType::kSystem};
    case SymbolWifi:
      return {@"wifi", SymbolType::kSystem};
    case SymbolWork:
      return {@"case", SymbolType::kSystem};
    case SymbolWrenchAndScrewdriver:
      return {@"wrench.and.screwdriver", SymbolType::kSystem};
    case SymbolXMark:
      return {@"xmark", SymbolType::kSystem};
    case SymbolXMarkCircle:
      return {@"xmark.circle", SymbolType::kSystem};
    case SymbolXMarkCircleFill:
      return {@"xmark.circle.fill", SymbolType::kSystem};
    case SymbolXMarkSquare:
      return {@"xmark.square", SymbolType::kSystem};
    case SymbolXMarkSquareFill:
      return {@"xmark.square.fill", SymbolType::kSystem};
    case SymbolZoomTextAction:
      return {@"plus.magnifyingglass", SymbolType::kSystem};
  }
}
