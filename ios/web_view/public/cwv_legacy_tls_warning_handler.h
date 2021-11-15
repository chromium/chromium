// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_LEGACY_TLS_WARNING_HANDLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_LEGACY_TLS_WARNING_HANDLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Used to handle legacy TLS warnings. For example TLS 1.0 and 1.1.
CWV_EXPORT
@interface CWVLegacyTLSWarningHandler : NSObject

// The URL of the failing page.
@property(nonatomic, readonly) NSURL* URL;

// The NSError associated with this warning.
@property(nonatomic, readonly) NSError* error;

- (instancetype)init NS_UNAVAILABLE;

// Call to display a warning page with |HTML|.
// This will no op after the first call.
- (void)displayWarningPageWithHTML:(NSString*)HTML;

// Call to override the warning and reload the page.
// This will no op after the first call.
- (void)overrideWarningAndReloadPage;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_LEGACY_TLS_WARNING_HANDLER_H_
