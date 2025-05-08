// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARED_TAB_GROUP_LAST_TAB_CLOSED_ALERT_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARED_TAB_GROUP_LAST_TAB_CLOSED_ALERT_COMMAND_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class Browser;
class TabGroup;
enum class TabGroupActionType;

namespace web {
class WebStateID;
}

// Payload object for the command to trigger the last tab in a shared group
// alert.
@interface SharedTabGroupLastTabAlertCommand : NSObject

// Creates the command with the last tab's information (`tabID`, `browser`,
// `group`) and a boolean `closing` to indicate if the alert is due a tab
// closure by setting it to YES and NO when it's due to a tab move.
- (instancetype)initWithTabID:(web::WebStateID)tabID
                      browser:(Browser*)browser
                        group:(const TabGroup*)group
           baseViewController:(UIViewController*)baseViewController
                   sourceView:(UIView*)sourceView
                      closing:(BOOL)closing;
- (instancetype)init NS_UNAVAILABLE;

// Group to potentially delete, leave or keep.
@property(nonatomic, readonly) const TabGroup* group;
// The title of the group.
@property(nonatomic, readonly, copy) NSString* groupTitle;
// Last tab ID.
@property(nonatomic, readonly) web::WebStateID tabID;
// Type of the action.
@property(nonatomic, readonly, assign) TabGroupActionType actionType;
// Base view controller where the alert will be displayed.
@property(nonatomic, readonly, strong) UIViewController* baseViewController;
// Source view where the UI is attached.
@property(nonatomic, readonly, strong) UIView* sourceView;
// YES if it should be display as an alert instead of being anchored to the
// source view.
@property(nonatomic, readonly, assign) BOOL displayAsAlert;
// YES if the alert should offer the possibility to cancel the action.
@property(nonatomic, readonly, assign) BOOL canCancel;
// YES if the alert is coming from a closing action, NO if it is a move action.
@property(nonatomic, readonly, assign) BOOL closing;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARED_TAB_GROUP_LAST_TAB_CLOSED_ALERT_COMMAND_H_
