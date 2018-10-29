// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/signin/chromium_signin_resources_provider.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumSigninResourcesProvider::ChromiumSigninResourcesProvider() {}
ChromiumSigninResourcesProvider::~ChromiumSigninResourcesProvider() {}

// The signin code expects to receive a non-nil response to this method, even
// though signin is not supported by Chromium builds.
UIImage* ChromiumSigninResourcesProvider::GetDefaultAvatar() {
  return ImageWithColor([UIColor lightGrayColor]);
}

NSString* ChromiumSigninResourcesProvider::GetLocalizedString(
    ios::SigninStringID string_id) {
  return @"";
}
