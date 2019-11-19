// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NETWORK_ACTIVITY_NETWORK_ACTIVITY_INDICATOR_MANAGER_H_
#define IOS_CHROME_BROWSER_NETWORK_ACTIVITY_NETWORK_ACTIVITY_INDICATOR_MANAGER_H_

#import <Foundation/Foundation.h>

// This class controls access to the network activity indicator across the
// app. It provides a simple interface for clients to indicate they are
// starting a network task and they would like the indicator shown, and to
// indicate they have finished a network task.
//
// Clients are required to pass an NSString* to each method to identify
// themselves. Separating clients into groups prevents a client from "stopping"
// requests from other clients on accident, and makes those bugs easier to
// track down. Specifically, the manager will immediately fail if the number
// of tasks stopped for a group ever exceeds the number of tasks started for
// that group. Clients are responsible for namespacing their group strings
// properly. All methods must be called on the UI thread.
@interface NetworkActivityIndicatorManager : NSObject

// Returns the singleton NetworkActivityIndicatorManager.
+ (NetworkActivityIndicatorManager*)sharedInstance;

// Begins a single network task. The network activity indicator is guaranteed
// to be shown after this finishes (if it isn't already). |group| must be
// non-nil.
- (void)startNetworkTaskForGroup:(NSString*)group;

// Stops a single network task. The network activity indicator may or may not
// stop being shown once this finishes, depending on whether there are other
// unstopped tasks or not. |group| must be non-nil, and have at least one
// unstopped task.
- (void)stopNetworkTaskForGroup:(NSString*)group;

// A convenience method for starting multiple network tasks at once. |group|
// must be non-nil. |numTasks| must be greater than 0.
- (void)startNetworkTasks:(NSUInteger)numTasks forGroup:(NSString*)group;

// A convenience method for stopping multiple network tasks at once. |group|
// must be non-nil. |numTasks| must be greater than 0, and |numTasks| must be
// less than or equal to the number of unstopped tasks in |group|.
- (void)stopNetworkTasks:(NSUInteger)numTasks forGroup:(NSString*)group;

// A convenience method for stopping all network tasks for a group. |group|
// must be non-nil. Can be called on any group at any time, regardless of
// whether the group has any unstopped network tasks or not. Returns the number
// of tasks stopped by this call.
- (NSUInteger)clearNetworkTasksForGroup:(NSString*)group;

// Returns the number of unstopped network tasks for |group|. |group| must be
// non-nil. Can be called on any group at any time, regardless of whether the
// group has any unstopped network tasks or not.
- (NSUInteger)numNetworkTasksForGroup:(NSString*)group;

// Returns the total number of unstopped network tasks, across all groups. This
// method was added for testing only. Clients should never depend on this, and
// should instead only be concerned with the number of unstopped network tasks
// for the groups they control, which can be queried using
// |-numNetworkTasksForGroup:|.
- (NSUInteger)numTotalNetworkTasks;

@end

#endif  // IOS_CHROME_BROWSER_NETWORK_ACTIVITY_NETWORK_ACTIVITY_INDICATOR_MANAGER_H_
