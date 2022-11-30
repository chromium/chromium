// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_HELP_AND_FEEDBACK_H_
#define REMOTING_IOS_APP_HELP_AND_FEEDBACK_H_

#import <UIKit/UIKit.h>

#include "remoting/client/feedback_data.h"

// This is the base class to provide help and feedback functionalities.
@interface HelpAndFeedback : NSObject

// This will present the Send Feedback view controller onto the topmost view
// controller.
// context: a unique identifier for the user's place within the app which can be
// used to categorize the feedback report and segment usage metrics.
// The base implementation simply calls
// presentFeedbackFlowWithContext:feedbackData: with empty feedback data.
- (void)presentFeedbackFlowWithContext:(NSString*)context;

// Presents a feedback view controller with extra feedback data.
// The base implementation does nothing.
- (void)presentFeedbackFlowWithContext:(NSString*)context
                          feedbackData:(const remoting::FeedbackData&)data;

// Instance can only be set once.
@property(nonatomic, class) HelpAndFeedback* instance;

@end

#endif  // REMOTING_IOS_APP_HELP_AND_FEEDBACK_H_
