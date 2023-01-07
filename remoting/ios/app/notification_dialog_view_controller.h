// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_NOTIFICATION_DIALOG_VIEW_CONTROLLER_H_
#define REMOTING_IOS_APP_NOTIFICATION_DIALOG_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace remoting {
struct NotificationMessage;
}  // namespace remoting

using NotificationDialogCompletionBlock = void (^)(BOOL silenced);

// This is the view controller for showing the notification dialog.
@interface NotificationDialogViewController : UIViewController

- (instancetype)initWithNotificationMessage:
                    (const remoting::NotificationMessage&)message
                               allowSilence:(BOOL)allowSilence;

- (void)presentOnTopVCWithCompletion:
    (NotificationDialogCompletionBlock)completion;

@end

#endif  // REMOTING_IOS_APP_NOTIFICATION_DIALOG_VIEW_CONTROLLER_H_
