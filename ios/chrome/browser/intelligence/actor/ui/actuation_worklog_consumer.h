// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSUMER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ActuationWorklogChip;
@class ActuationWorklogItem;

// Consumer protocol for receiving worklog updates.
@protocol ActuationWorklogConsumer <NSObject>

// Sets whether actuation is actively executing.
- (void)setActuationActive:(BOOL)active;

// Sets the task title displayed in the header.
- (void)setTaskTitle:(NSString*)taskTitle;

// Called when a new actuation worklog step and its chip are ready to display.
- (void)updateWorklogWithItem:(ActuationWorklogItem*)item
                         chip:(ActuationWorklogChip*)chip
                     animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_CONSUMER_H_
