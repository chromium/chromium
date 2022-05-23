// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_
#define IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_

#import <UIKit/UIKit.h>

// Custom symbol names.
extern NSString* const kArrowClockWiseSymbol;
extern NSString* const kIncognitoSymbol;
extern NSString* const kIncognitoCircleFillSymbol;
extern NSString* const kSquareNumberSymbol;
extern NSString* const kTranslateSymbol;
extern NSString* const kCameraSymbol;
extern NSString* const kCameraFillSymbol;
extern NSString* const kPlusCircleFillSymbol;
extern NSString* const kArrowTriangleSlashCirclePathSymbol;
extern NSString* const kPopupBadgeMinusSymbol;

// Default symbol names.
extern NSString* const kCreditCardSymbol;
extern NSString* const kMicrophoneFillSymbol;
extern NSString* const kMicrophoneSymbol;
extern NSString* const kEllipsisCircleFillSymbol;
extern NSString* const kPinSymbol;
extern NSString* const kPinFillSymbol;
extern NSString* const kGearShapeSymbol;
extern NSString* const kShareSymbol;
extern NSString* const kXMarkSymbol;
extern NSString* const kPlusSymbol;
extern NSString* const kSearchSymbol;
extern NSString* const kCheckmarkSymbol;
extern NSString* const kArrowDownCircleFillSymbol;
extern NSString* const kWarningSymbol;
extern NSString* const kWarningFillSymbol;
extern NSString* const kHelpFillSymbol;
extern NSString* const kCheckMarkCircleSymbol;
extern NSString* const kCheckMarkCircleFillSymbol;
extern NSString* const kFailMarkCircleFillSymbol;
extern NSString* const kTrashSymbol;
extern NSString* const kInfoCircleSymbol;

// Returns a SF symbol named |symbolName| configured with the given
// |configuration|.
UIImage* DefaultSymbolWithConfiguration(NSString* symbolName,
                                        UIImageConfiguration* configuration);

// Returns a custom symbol named |symbolName| configured with the given
// |configuration|.
UIImage* CustomSymbolWithConfiguration(NSString* symbolName,
                                       UIImageConfiguration* configuration);

// Returns a SF symbol named |symbolName| configured with the default
// configuration and the given |pointSize|.
UIImage* DefaultSymbolWithPointSize(NSString* symbolName, NSInteger pointSize);

// Returns a custom symbol named |symbolName| configured with the default
// configuration and the given |pointSize|.
UIImage* CustomSymbolWithPointSize(NSString* symbolName, NSInteger pointSize);

// Returns a SF symbol named |symbolName| as a template image, configured with
// the default configuration and the given |pointSize|.
UIImage* DefaultSymbolTemplateWithPointSize(NSString* symbolName,
                                            NSInteger pointSize);

// Returns a custom symbol named |symbolName| as a template image, configured
// with the default configuration and the given |pointSize|.
UIImage* CustomSymbolTemplateWithPointSize(NSString* symbolName,
                                           NSInteger pointSize);

// Returns YES if the kUseSFSymbols flag is enabled.
bool UseSymbols();

#endif  // IOS_CHROME_BROWSER_UI_ICONS_CHROME_SYMBOL_H_
