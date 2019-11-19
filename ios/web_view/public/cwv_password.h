// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_PASSWORD_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_PASSWORD_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Represents a password for autofilling login forms.
CWV_EXPORT
@interface CWVPassword : NSObject

// Display friendly title for this object.
@property(nonatomic, copy, readonly) NSString* title;
// The url for which this password can be used in its login form.
@property(nonatomic, copy, readonly) NSString* site;

// Whether or not |site| has been blacklisted by the user. This means password
// autofill will never occur.
@property(nonatomic, readonly, getter=isBlacklisted) BOOL blacklisted;

// The login username. This is nil if |blacklisted| or if only the |password|
// was saved.
@property(nonatomic, nullable, copy, readonly) NSString* username;
// The login password. This is nil iff |blacklisted|.
// Note that you should only display this after the user authenticates via iOS'
// native authentication mechanism. e.g. Passcode and/or Touch/Face ID.
@property(nonatomic, nullable, copy, readonly) NSString* password;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_PASSWORD_H_
