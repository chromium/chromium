// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_UNSAFE_URL_HANDLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_UNSAFE_URL_HANDLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Threat types.
typedef NS_ENUM(NSInteger, CWVUnsafeURLThreatType) {
  // Unknown threat type.
  CWVUnsafeURLThreatTypeUnknown = 0,
  // Potential billing that may not be obvious to the user.
  CWVUnsafeURLThreatTypeBilling,
  // The URL hosts malware.
  CWVUnsafeURLThreatTypeMalware,
  // The URL hosts unwanted programs.
  CWVUnsafeURLThreatTypeUnwanted,
  // The URL is being used for phishing.
  CWVUnsafeURLThreatTypePhishing,
};

// Used to make a decision on whether or not to load an unsafe URL.
// Unsafe URLs are URLs known for phishing, hosting malware, etc.
// The client should block user-initiated navigation until this
// handler is resolved.
CWV_EXPORT
@interface CWVUnsafeURLHandler : NSObject

// The URL in the main frame that originated the load of an unsafe URL.
@property(nonatomic, readonly) NSURL* mainFrameURL;

// The requested URL that is considered unsafe.
// This may be the same as |mainFrameURL|.
@property(nonatomic, readonly) NSURL* requestURL;

// The threat type of the unsafe URL.
@property(nonatomic, readonly) CWVUnsafeURLThreatType threatType;

- (instancetype)init NS_UNAVAILABLE;

// Display an interstitial page with |HTML|.
// This will be a no-op after the first call.
- (void)displayInterstitialPageWithHTML:(NSString*)HTML;

// Proceeds to the unsafe URL anyways.
- (void)proceed;

// Navigates back to safety.
- (void)goBack;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_UNSAFE_URL_HANDLER_H_
