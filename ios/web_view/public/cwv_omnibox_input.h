// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_OMNIBOX_INPUT_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_OMNIBOX_INPUT_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// A detected type of a user input to Omnibox.
typedef NS_ENUM(NSInteger, CWVOmniboxInputType) {
  // Empty input.
  CWVOmniboxInputTypeEmpty,
  // Valid input whose type cannot be determined.
  CWVOmniboxInputTypeUnknown,
  // Input autodetected as a URL.
  CWVOmniboxInputTypeURL,
  // Input autodetected as a query.
  CWVOmniboxInputTypeQuery,
};

// A protocol for properties of CWVOmniboxInput.
//
// Provided as a way to provide fake of CWVOmniboxInput.
@protocol CWVOmniboxInputProtocol <NSObject>

// The given user input text.
@property(nonatomic, readonly) NSString* text;

// The detected type of a user input.
@property(nonatomic, readonly) CWVOmniboxInputType type;

// The input as a URL to navigate to, if possible.
//
// It is not nil when `type` is CWVOmniboxInputTypeURL. Otherwise it may be nil.
@property(nonatomic, readonly, nullable) NSURL* URL;

// YES if HTTPS scheme was added to `URL` because the scheme was omitted in
// `text` (e.g., @"example.com") and `shouldUseHTTPSAsDefaultScheme` was set to
// YES.
@property(nonatomic, readonly) BOOL addedHTTPSToTypedURL;

@end

// Parsed result of a user input to Omnibox.
CWV_EXPORT
@interface CWVOmniboxInput : NSObject <CWVOmniboxInputProtocol>

// `inputText`: The user input.
// `shouldUseHTTPSAsDefaultScheme`: When YES is specified, HTTPS is assumed when
// the input is URL and the scheme is omitted e.g., @"example.com".
- (instancetype)initWithText:(NSString*)text
    shouldUseHTTPSAsDefaultScheme:(BOOL)shouldUseHTTPSAsDefaultScheme
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_OMNIBOX_INPUT_H_
