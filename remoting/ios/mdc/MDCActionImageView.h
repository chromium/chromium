// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_MDC_MDCACTIONIMAGEVIEW_H_
#define REMOTING_IOS_MDC_MDCACTIONIMAGEVIEW_H_

// TODO(nicholss): Currently working on getting this into MDC for iOS, at that
// time we will remove this version from chromium.

#import <UIKit/UIKit.h>

// This is the image view for Floating action buttons to provide the icon
// rotation animation and displaying of the primary or active image based on
// active state.
@interface MDCActionImageView : UIView

- (id)initWithFrame:(CGRect)frame
       primaryImage:(UIImage*)primary
        activeImage:(UIImage*)active;

- (void)setActive:(BOOL)active animated:(BOOL)animated;

@property(nonatomic) BOOL active;

@end

#endif  // REMOTING_IOS_MDC_MDCACTIONIMAGEVIEW_H_
