// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_IDENTITY_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_IDENTITY_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Represents a user in ChromeWebView.
CWV_EXPORT
@interface CWVIdentity : NSObject

- (instancetype)initWithEmail:(nullable NSString*)email
                     fullName:(nullable NSString*)fullName
                       gaiaID:(NSString*)gaiaID NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The user's email address. e.g. john.doe@chromium.org.
@property(nonatomic, copy, readonly, nullable) NSString* email;

// The user's full name. e.g. John Doe.
@property(nonatomic, copy, readonly, nullable) NSString* fullName;

// The unique GAIA (Google Accounts ID Administration) ID for this user.
@property(nonatomic, copy, readonly) NSString* gaiaID;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_IDENTITY_H_
