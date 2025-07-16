// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_GLOBAL_STATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_GLOBAL_STATE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Flags that specify options that should be set before starting the early
// initialization.
CWV_EXPORT
@interface CWVEarlyInitFlags : NSObject

// Set to `YES` to enable the autofill across iframes feature.
@property(nonatomic, readwrite) BOOL autofillAcrossIframesEnabled;

@end

// Manages internal global state that must be initialized before accessing any
// of the other public APIs //ios/web_view/public.
CWV_EXPORT
@interface CWVGlobalState : NSObject

@property(class, nonatomic, readonly) CWVGlobalState* sharedInstance;

// Allows full customization of the user agent for all requests.
// If non-nil, this is used instead of `userAgentProduct`.
@property(nonatomic, copy, nullable) NSString* customUserAgent;

// Customizes the User Agent string by inserting `userAgentProduct`.
// It should be of the format "product/1.0". For example:
// "Mozilla/5.0 (iPhone; CPU iPhone OS 10_3 like Mac OS X) AppleWebKit/603.1.30
// (KHTML, like Gecko) <product> Mobile/16D32 Safari/602.1" where <product>
// will be replaced with `userAgentProduct` or empty string if not set.
//
// Deprecated. Use `customUserAgent` instead.
@property(nonatomic, copy, nullable) NSString* userAgentProduct;

// Returns `YES` if `-[CWVGlobalState earlyInit]` has been called.
@property(nonatomic, readonly, getter=isEarlyInitialized) BOOL earlyInitialized;

// Returns `YES` if `-[CWVGlobalState start]` has been called.
@property(nonatomic, readonly, getter=isStarted) BOOL started;

// Returns `YES` if the autofill across iframes feature is enabled.
@property(nonatomic, readonly) BOOL autofillAcrossIframesEnabled;

- (instancetype)init NS_UNAVAILABLE;

// Use this method to set the necessary credentials used to communicate with
// the Google API for features such as translate. See this link for more info:
// https://support.google.com/googleapi/answer/6158857
- (void)setGoogleAPIKey:(NSString*)googleAPIKey
               clientID:(NSString*)clientID
           clientSecret:(NSString*)clientSecret;

// Initializes internal global state machinery with the default flags. See
// -earlyInitWithFlags for more details.
- (void)earlyInit;

// Initializes internal global state machinery with `flags` specifying options
// that should be set before starting the early initialization. This should be
// called as early as possible during the host app's launch process. For
// example, in
// `-[UIApplicationDelegate application:willFinishLaunchingWithOptions:]`.
- (void)earlyInitWithFlags:(CWVEarlyInitFlags*)flags;

// Starts up internal global state machinery. This can be called anytime after
// `earlyInit`, but must be called before using any other CWV* classes.
- (void)start;

// Stops and deinitializes internal global state machinery. This should be
// called when the OS terminates the host app. i.e., in
// `-[UIApplicationDelegate applicationWillTerminate:]`.
- (void)stop;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_GLOBAL_STATE_H_
