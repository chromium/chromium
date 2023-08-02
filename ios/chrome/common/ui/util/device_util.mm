// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/device_util.h"

#import <UIKit/UIKit.h>
#import "ios/chrome/common/constants.h"

CGFloat CurrentScreenHeight() {
  return [UIScreen mainScreen].bounds.size.height;
}

CGFloat CurrentScreenWidth() {
  return [UIScreen mainScreen].bounds.size.width;
}

bool IsSmallDevice() {
  CGSize mSize = [@"m" sizeWithAttributes:@{
    NSFontAttributeName : [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
  }];
  CGFloat emWidth = CurrentScreenWidth() / mSize.width;
  return emWidth < kSmallDeviceThreshold;
}