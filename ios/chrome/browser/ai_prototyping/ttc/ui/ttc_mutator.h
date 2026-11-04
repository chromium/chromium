// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_MUTATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_MUTATOR_H_

#import <Foundation/Foundation.h>

// Filter options for the session journal in the TTC prototype UI.
enum class TTCJournalFilter {
  kAll = 0,
  kSession,
  kConversation,
  kBackend,
  kAudio,
  kActuation,
};

// Protocol for mutating model and hardware state based on user interactions
// in the TTC prototype UI.
@protocol TTCMutator <NSObject>

// Notifies the mutator that the UI has appeared, triggering state hydration.
- (void)viewWillAppear;

// Clears all entries in the session journal.
- (void)clearJournal;

// Filters the session journal by `filter`.
- (void)setJournalComponentFilter:(TTCJournalFilter)filter;

// Toggles between the simplified human-readable journal view (`NO`, default)
// and the detailed telemetry journal view (`YES`).
- (void)setJournalDetailedMode:(BOOL)detailed;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_MUTATOR_H_
