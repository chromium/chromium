// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_MESSAGE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_MESSAGE_H_

#import <MaterialComponents/MaterialSnackbar.h>

// A snackbar message that contains profile information of the current logged in
//  account.
@interface IdentitySnackbarMessage : MDCSnackbarMessage

// The avatar to display in the snackbar. Must not be nil.
@property(nonatomic, readonly) UIImage* avatar;

// The name to display in the snackbar. May be nil, in which case the snackbar
// will not display a name.
@property(nonatomic, readonly) NSString* name;

// The email to display in the snackbar. Must not be nil.
@property(nonatomic, readonly) NSString* email;

// True if the profile is managed by an enterprise admin.
@property(nonatomic, readonly) BOOL managed;

- (instancetype)initWithName:(NSString*)name
                       email:(NSString*)email
                      avatar:(UIImage*)avatar
                     managed:(BOOL)managed;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_IDENTITY_SNACKBAR_IDENTITY_SNACKBAR_MESSAGE_H_
