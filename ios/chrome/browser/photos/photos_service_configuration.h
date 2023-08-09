// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@protocol SingleSignOnService;

// Configuration object used by the PhotosService.
@interface PhotosServiceConfiguration : NSObject

// The SingleSignOnService instance to use by PhotosService.
@property(nonatomic, strong) id<SingleSignOnService> ssoService;

@end

#endif  // IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_CONFIGURATION_H_
