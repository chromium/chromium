// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SHARE_EXTENSION_ACCOUNT_INFO_H_
#define IOS_CHROME_SHARE_EXTENSION_ACCOUNT_INFO_H_

#import <UIKit/UIKit.h>

@interface AccountInfo : NSObject
@property(nonatomic, copy) NSString* gaiaIDString;
@property(nonatomic, copy) NSString* fullName;
@property(nonatomic, copy) NSString* email;
@property(nonatomic, copy) UIImage* avatar;
@end

#endif  // IOS_CHROME_SHARE_EXTENSION_ACCOUNT_INFO_H_
