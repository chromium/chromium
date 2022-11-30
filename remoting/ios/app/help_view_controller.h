// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HELP_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_HELP_VIEW_CONTROLLER_H_

#import <SafariServices/SafariServices.h>
#import <UIKit/UIKit.h>

// A VC that shows the help center.
@interface HelpViewController : SFSafariViewController

- (instancetype)init;

@end

#endif  // REMOTING_IOS_APP_HELP_VIEW_CONTROLLER_H_
