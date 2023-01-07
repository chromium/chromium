// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_LOOKALIKE_URL_HANDLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_LOOKALIKE_URL_HANDLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Enumerates possible decisions that can be made.
typedef NS_ENUM(NSInteger, CWVLookalikeURLHandlerDecision) {
  // Continue navigation to the original requested URL.
  CWVLookalikeURLHandlerDecisionProceedToRequestURL,
  // Change navigation to the suggested safe URL, if any.
  CWVLookalikeURLHandlerDecisionProceedToSafeURL,
  // Navigates back or closes the web view if there's nothing to go back to.
  CWVLookalikeURLHandlerDecisionGoBackOrClose,
};

// Used to make a decision on whether or not to load a lookalike URL.
// The client should block other user decisions, like manually navigating back,
// until this handler is resolved by |commitDecision:|.
CWV_EXPORT
@interface CWVLookalikeURLHandler : NSObject

// The requested URL that looks like some other well known URL.
@property(nonatomic, readonly) NSURL* requestURL;

// The URL that |requestURL| may have been pretending to be.
// May be nil if there is no safe alternative.
@property(nonatomic, nullable, readonly) NSURL* safeURL;

- (instancetype)init NS_UNAVAILABLE;

// Display an interstitial page with |HTML|.
// This will be a no-op after the first call.
- (void)displayInterstitialPageWithHTML:(NSString*)HTML;

// Resolves the handler by taking the action indicated by |decision|.
// Returns a BOOL indicating if the decision was accepted.
// Returns NO if you attempt to proceed to a nil |safeURL|.
// This handler is considered resolved after the first call that returns YES,
// and will be no-op thereafter
- (BOOL)commitDecision:(CWVLookalikeURLHandlerDecision)decision;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_LOOKALIKE_URL_HANDLER_H_
