// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_ENTRIES_DIFF_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_ENTRIES_DIFF_H_

#import <Foundation/Foundation.h>

@class ActuationWorklogEntry;

// Describes how to update a worklog view from the entries it displays to new
// entries. Worklog views can only append entries or replace the last one, so
// any other change requires a reset.
struct ActuationWorklogEntriesDiff {
  // Whether the view must be cleared before appending. If true, `appended`
  // holds every new entry.
  bool reset = false;
  // New version of the last displayed entry, if it changed.
  ActuationWorklogEntry* replaced_last = nil;
  // Entries to append, in display order. Never nil.
  NSArray<ActuationWorklogEntry*>* appended = nil;
};

// Returns the changes that turn `old_entries` (nil if nothing is displayed)
// into `new_entries`. Entries are compared by pointer, as the same instance is
// kept across states.
ActuationWorklogEntriesDiff ComputeActuationWorklogEntriesDiff(
    NSArray<ActuationWorklogEntry*>* old_entries,
    NSArray<ActuationWorklogEntry*>* new_entries);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_ENTRIES_DIFF_H_
