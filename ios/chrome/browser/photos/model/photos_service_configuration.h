// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class ChromeAccountManagerService;
class PrefService;
@protocol SingleSignOnService;

namespace signin {
class IdentityManager;
}  // namespace signin

// Configuration object used by the PhotosService.
@interface PhotosServiceConfiguration : NSObject

// The SingleSignOnService instance to use by PhotosService.
@property(nonatomic, strong) id<SingleSignOnService> singleSignOnService;
// PrefService to check the state of Save to Photos preferences.
@property(nonatomic, assign) PrefService* prefService;
// IdentityManager to check whether the user is signed-in.
@property(nonatomic, assign) signin::IdentityManager* identityManager;
// ChromeAccountManagerService to get list of accounts on the device.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

#endif  // IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_CONFIGURATION_H_
