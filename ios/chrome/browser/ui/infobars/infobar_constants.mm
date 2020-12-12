// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const int kInfobarBackgroundColor = 0xfafafa;

// a11y identifier so that automation can tap on either infobar button
NSString* const kConfirmInfobarButton1AccessibilityIdentifier =
    @"confirmInfobarButton1AXID";
NSString* const kConfirmInfobarButton2AccessibilityIdentifier =
    @"confirmInfobarButton2AXID";

const NSTimeInterval kInfobarBannerDefaultPresentationDurationInSeconds = 8.0;
const NSTimeInterval kInfobarBannerLongPresentationDurationInSeconds = 15.0;
