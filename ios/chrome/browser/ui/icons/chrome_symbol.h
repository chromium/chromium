// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_
#define IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/icons/buildflags.h"

// Custom symbol names.
extern NSString* const kArrowClockWiseSymbol;
extern NSString* const kSquareNumberSymbol;
extern NSString* const kTranslateSymbol;
extern NSString* const kCameraSymbol;
extern NSString* const kCameraFillSymbol;
extern NSString* const kPasswordManagerSymbol;
extern NSString* const kPlusCircleFillSymbol;
extern NSString* const kPopupBadgeMinusSymbol;
extern NSString* const kPhotoBadgePlusSymbol;
extern NSString* const kPhotoBadgeMagnifyingglassSymbol;
extern NSString* const kReadingListSymbol;
extern NSString* const kRecentTabsSymbol;
extern NSString* const kLanguageSymbol;
extern NSString* const kPasswordSymbol;
extern NSString* const kCameraLensSymbol;
extern NSString* const kDownTrendSymbol;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
extern NSString* const kGoogleShieldSymbol;
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
extern NSString* const kShieldSymbol;

// Default symbol names.
extern NSString* const kCreditCardSymbol;
extern NSString* const kMicrophoneFillSymbol;
extern NSString* const kMicrophoneSymbol;
extern NSString* const kEllipsisCircleFillSymbol;
extern NSString* const kPinSymbol;
extern NSString* const kPinFillSymbol;
extern NSString* const kSettingsSymbol;
extern NSString* const kSettingsFilledSymbol;
extern NSString* const kShareSymbol;
extern NSString* const kXMarkSymbol;
extern NSString* const kPlusSymbol;
extern NSString* const kSearchSymbol;
extern NSString* const kCheckmarkSymbol;
extern NSString* const kDownloadSymbol;
extern NSString* const kSecureSymbol;
extern NSString* const kWarningSymbol;
extern NSString* const kWarningFillSymbol;
extern NSString* const kHelpSymbol;
extern NSString* const kCheckMarkCircleSymbol;
extern NSString* const kCheckMarkCircleFillSymbol;
extern NSString* const kFailMarkCircleFillSymbol;
extern NSString* const kTrashSymbol;
extern NSString* const kInfoCircleSymbol;
extern NSString* const kHistorySymbol;
extern NSString* const kCheckmarkSealSymbol;
extern NSString* const kWifiSymbol;
extern NSString* const kBookmarksSymbol;
extern NSString* const kSyncErrorSymbol;
extern NSString* const kMenuSymbol;
extern NSString* const kSortSymbol;
extern NSString* const kBackSymbol;
extern NSString* const kForwardSymbol;
extern NSString* const kPersonFillSymbol;
extern NSString* const kMailFillSymbol;
extern NSString* const kPhoneFillSymbol;

// Names of the default symbol being non-monochrome by default. When using them,
// you probably want to set their color to monochrome.
extern NSString* const kIPhoneSymbol;
extern NSString* const kIPadSymbol;
extern NSString* const kLaptopSymbol;

// The corner radius of the symbol with a colorful background.
extern const CGFloat kColorfulBackgroundSymbolCornerRadius;

// Returns a SF symbol named `symbol_name` configured with the given
// `configuration`.
UIImage* DefaultSymbolWithConfiguration(NSString* symbol_name,
                                        UIImageConfiguration* configuration);

// Returns a custom symbol named `symbol_name` configured with the given
// `configuration`.
UIImage* CustomSymbolWithConfiguration(NSString* symbol_name,
                                       UIImageConfiguration* configuration);

// Returns a SF symbol named `symbol_name` configured with the default
// configuration and the given `point_size`.
UIImage* DefaultSymbolWithPointSize(NSString* symbol_name, CGFloat point_size);

// Returns a custom symbol named `symbol_name` configured with the default
// configuration and the given `point_size`.
UIImage* CustomSymbolWithPointSize(NSString* symbol_name, CGFloat point_size);

// Returns a SF symbol named `symbol_name` as a template image, configured with
// the default configuration and the given `point_size`.
UIImage* DefaultSymbolTemplateWithPointSize(NSString* symbol_name,
                                            CGFloat point_size);

// Returns a custom symbol named `symbol_name` as a template image, configured
// with the default configuration and the given `point_size`.
UIImage* CustomSymbolTemplateWithPointSize(NSString* symbol_name,
                                           CGFloat point_size);

// Returns the given `symbol`, making sure that it is preferring the monochrome
// version.
UIImage* MakeSymbolMonochrome(UIImage* symbol);

// Returns the given `symbol`, making sure that it is preferring the multicolor
// version.
UIImage* MakeSymbolMulticolor(UIImage* symbol);

// Returns the given `symbol`, with a "Palette" of `colors`.
UIImage* ConfigureSymbolWithPaletteColors(UIImage* symbol,
                                          NSArray<UIColor*>* colors)
    API_AVAILABLE(ios(15.0));

// Returns YES if the kUseSFSymbols flag is enabled.
bool UseSymbols();

#endif  // IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_
