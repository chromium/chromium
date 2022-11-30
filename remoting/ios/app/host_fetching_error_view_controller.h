// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HOST_FETCHING_ERROR_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_HOST_FETCHING_ERROR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// This VC shows a dialog-like view to allow the user to retry host list
// fetching. This is used when the host list has never been successfully,
// fetched, i.e. pull-to-refresh is not available.
@interface HostFetchingErrorViewController : UIViewController

// Called when the retry button is tapped.
@property(nonatomic) void (^onRetryCallback)();

// Returns the label that shows the error message.
@property(nonatomic, readonly) UILabel* label;

@end

#endif  // REMOTING_IOS_APP_HOST_FETCHING_ERROR_VIEW_CONTROLLER_H_
