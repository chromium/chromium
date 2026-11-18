// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_entries_diff.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Creates an entry displaying `text`.
ActuationWorklogEntry* Entry(NSString* text) {
  return [[ActuationWorklogEntry alloc]
      initWithItem:[ActuationWorklogItem freeformItemWithText:text]
              chip:nil];
}

using ActuationWorklogEntriesDiffTest = PlatformTest;

// Tests that every entry is appended when nothing is displayed.
TEST_F(ActuationWorklogEntriesDiffTest, AppendsAllWhenNothingDisplayed) {
  ActuationWorklogEntry* first = Entry(@"first");
  ActuationWorklogEntry* second = Entry(@"second");

  ActuationWorklogEntriesDiff diff =
      ComputeActuationWorklogEntriesDiff(nil, @[ first, second ]);

  EXPECT_FALSE(diff.reset);
  EXPECT_EQ(nil, diff.replaced_last);
  EXPECT_NSEQ((@[ first, second ]), diff.appended);
}

// Tests that only the entries following the displayed ones are appended.
TEST_F(ActuationWorklogEntriesDiffTest, AppendsNewEntries) {
  ActuationWorklogEntry* first = Entry(@"first");
  ActuationWorklogEntry* second = Entry(@"second");
  ActuationWorklogEntry* third = Entry(@"third");

  ActuationWorklogEntriesDiff diff =
      ComputeActuationWorklogEntriesDiff(@[ first ], @[ first, second, third ]);

  EXPECT_FALSE(diff.reset);
  EXPECT_EQ(nil, diff.replaced_last);
  EXPECT_NSEQ((@[ second, third ]), diff.appended);
}

// Tests that a changed last entry is replaced before new entries are appended.
TEST_F(ActuationWorklogEntriesDiffTest, ReplacesLastEntryAndAppends) {
  ActuationWorklogEntry* first = Entry(@"first");
  ActuationWorklogEntry* second = Entry(@"second");
  ActuationWorklogEntry* updated_second = Entry(@"updated_second");
  ActuationWorklogEntry* third = Entry(@"third");

  ActuationWorklogEntriesDiff diff = ComputeActuationWorklogEntriesDiff(
      @[ first, second ], @[ first, updated_second, third ]);

  EXPECT_FALSE(diff.reset);
  EXPECT_EQ(updated_second, diff.replaced_last);
  EXPECT_NSEQ(@[ third ], diff.appended);
}

// Tests that a changed last entry is replaced when no entries are added.
TEST_F(ActuationWorklogEntriesDiffTest, ReplacesLastEntryOnly) {
  ActuationWorklogEntry* first = Entry(@"first");
  ActuationWorklogEntry* second = Entry(@"second");
  ActuationWorklogEntry* updated_second = Entry(@"updated_second");

  ActuationWorklogEntriesDiff diff = ComputeActuationWorklogEntriesDiff(
      @[ first, second ], @[ first, updated_second ]);

  EXPECT_FALSE(diff.reset);
  EXPECT_EQ(updated_second, diff.replaced_last);
  EXPECT_NSEQ(@[], diff.appended);
}

// Tests that unchanged entries produce no changes.
TEST_F(ActuationWorklogEntriesDiffTest, NoChangesForSameEntries) {
  ActuationWorklogEntry* first = Entry(@"first");
  ActuationWorklogEntry* second = Entry(@"second");

  ActuationWorklogEntriesDiff diff = ComputeActuationWorklogEntriesDiff(
      @[ first, second ], @[ first, second ]);

  EXPECT_FALSE(diff.reset);
  EXPECT_EQ(nil, diff.replaced_last);
  EXPECT_NSEQ(@[], diff.appended);
}

// Tests that a changed entry before the last one resets the display.
TEST_F(ActuationWorklogEntriesDiffTest, ResetsWhenEarlierEntryChanges) {
  ActuationWorklogEntry* first = Entry(@"first");
  ActuationWorklogEntry* second = Entry(@"second");
  ActuationWorklogEntry* updated_second = Entry(@"updated_second");
  ActuationWorklogEntry* third = Entry(@"third");

  ActuationWorklogEntriesDiff diff = ComputeActuationWorklogEntriesDiff(
      @[ first, second, third ], @[ first, updated_second, third ]);

  EXPECT_TRUE(diff.reset);
  EXPECT_EQ(nil, diff.replaced_last);
  EXPECT_NSEQ((@[ first, updated_second, third ]), diff.appended);
}

// Tests that removed entries reset the display.
TEST_F(ActuationWorklogEntriesDiffTest, ResetsWhenEntriesAreRemoved) {
  ActuationWorklogEntry* first = Entry(@"first");
  ActuationWorklogEntry* second = Entry(@"second");

  ActuationWorklogEntriesDiff diff =
      ComputeActuationWorklogEntriesDiff(@[ first, second ], @[ first ]);

  EXPECT_TRUE(diff.reset);
  EXPECT_EQ(nil, diff.replaced_last);
  EXPECT_NSEQ(@[ first ], diff.appended);
}

// Tests that clearing every entry resets the display, with nothing to append.
TEST_F(ActuationWorklogEntriesDiffTest, ResetsWhenAllEntriesAreRemoved) {
  ActuationWorklogEntriesDiff diff =
      ComputeActuationWorklogEntriesDiff(@[ Entry(@"first") ], nil);

  EXPECT_TRUE(diff.reset);
  EXPECT_EQ(nil, diff.replaced_last);
  EXPECT_NSEQ(@[], diff.appended);
}

}  // namespace
