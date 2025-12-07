// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_OBSCURED_INSETS_CONTROLLER_H_
#define IOS_WEB_COMMON_CRW_OBSCURED_INSETS_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Protocol for views that can have obscured insets.
@protocol CRWObscuredInsetsController <NSObject>

@property(nonatomic) UIEdgeInsets obscuredContentInsets API_AVAILABLE(ios(26.0))
    ;

@end

#endif  // IOS_WEB_COMMON_CRW_OBSCURED_INSETS_CONTROLLER_H_
