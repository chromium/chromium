// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_

#import <Foundation/Foundation.h>

// BWG UI sheet detent identifier.
extern NSString* const kBWGPromoConsentFullDetentIdentifier;

// BWG UI Lottie Animation name for FRE Banner.
extern NSString* const kLottieAnimationFREBannerName;

// Session map dictionary key for the last interaction timestamp.
extern const char kLastInteractionTimestampDictKey[];

// Session map dictionary key for the server ID.
extern const char kServerIDDictKey[];

// Session map dictionary key for the visible URL during the last BWG
// interaction.
extern const char kURLOnLastInteractionDictKey[];

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UTILS_BWG_CONSTANTS_H_
