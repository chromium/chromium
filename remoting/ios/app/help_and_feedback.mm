// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/help_and_feedback.h"

#import "base/check.h"
#import "base/notreached.h"

static HelpAndFeedback* g_helpAndFeedback;

@implementation HelpAndFeedback

#pragma mark - Public

- (void)presentFeedbackFlowWithContext:(NSString*)context {
  [self presentFeedbackFlowWithContext:context
                          feedbackData:remoting::FeedbackData()];
}

- (void)presentFeedbackFlowWithContext:(NSString*)context
                          feedbackData:(const remoting::FeedbackData&)data {
  NOTIMPLEMENTED() << "This should be implemented by a subclass.";
}

#pragma mark - Static Properties

+ (void)setInstance:(HelpAndFeedback*)instance {
  DCHECK(!g_helpAndFeedback);
  g_helpAndFeedback = instance;
}

+ (HelpAndFeedback*)instance {
  DCHECK(g_helpAndFeedback);
  return g_helpAndFeedback;
}

@end
