// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_FAVICON_PROVIDER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_FAVICON_PROVIDER_H_

#import <UIKit/UIKit.h>

class GURL;
@class FaviconAttributes;

// Interface for objects that can provide favicon images for URLs.
@protocol CredentialExportFaviconProvider <NSObject>

// Requests the receiver to provide a favicon image for `URL`. A `completion` is
// called synchronously with a FaviconAttributes instance if appropriate.
- (void)fetchFaviconForURL:(const GURL&)URL
                completion:(void (^)(FaviconAttributes*, BOOL))completion;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_FAVICON_PROVIDER_H_
