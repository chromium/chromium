// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate to handle SendTabToSelf Modal actions.
@protocol SendTabToSelfModalDelegate

// Asks the delegate to dismiss the modal dialog.
- (void)dismissViewControllerAnimated:(BOOL)animated
                           completion:(void (^)())completion;

// Asks the delegate to send the current tab to the device with |cacheGuid|.
- (void)sendTabToTargetDeviceCacheGUID:(NSString*)cacheGuid
                      targetDeviceName:(NSString*)deviceName;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODAL_DELEGATE_H_
