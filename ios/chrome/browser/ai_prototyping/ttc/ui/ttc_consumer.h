// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_CONSUMER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol receiving session journal telemetry to render in the TTC
// prototype UI.
@protocol TTCConsumer <NSObject>

// Replaces the displayed session journal log lines (used on initial hydration,
// filter change, or clear).
- (void)setJournalLines:(NSArray<NSString*>*)lines;

// Appends a single formatted line to the displayed session journal.
- (void)appendJournalLine:(NSString*)line;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_CONSUMER_H_
