// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_ERROR_PAGE_HELPER_H_
#define IOS_WEB_NAVIGATION_CRW_ERROR_PAGE_HELPER_H_

#import <Foundation/Foundation.h>

class GURL;

// Class used to create an Error Page, constructing all the information needed
// based on the initial error.
@interface CRWErrorPageHelper : NSObject

// Failed URL of the failed navigation.
@property(nonatomic, strong, readonly) NSURL* failedNavigationURL;
// The error page file to be loaded as a new page.
@property(nonatomic, strong, readonly) NSURL* errorPageFileURL;

- (instancetype)initWithError:(NSError*)error NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the failed URL if `URL` is an error page URL, otherwise empty URL.
+ (GURL)failedNavigationURLFromErrorPageFileURL:(const GURL&)URL;

// Returns whether `URL` is an error page URL.
+ (BOOL)isErrorPageFileURL:(const GURL&)URL;

// Returns a JavaScript script that can be injected to replace the content of
// the page with `HTML`. It can also contains a script to automatically reload
// the page when it is shown if `addAutomaticReload` is true.
- (NSString*)scriptForInjectingHTML:(NSString*)HTML
                 addAutomaticReload:(BOOL)addAutomaticReload;

// Returns YES if `URL` is a file URL for this error page.
- (BOOL)isErrorPageFileURLForFailedNavigationURL:(NSURL*)URL;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_ERROR_PAGE_HELPER_H_
