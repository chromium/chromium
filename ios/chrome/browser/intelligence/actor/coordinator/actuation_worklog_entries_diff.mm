// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_entries_diff.h"

namespace {

// Returns the number of leading entries `old_entries` and `new_entries` share.
NSUInteger SharedPrefixLength(NSArray<ActuationWorklogEntry*>* old_entries,
                              NSArray<ActuationWorklogEntry*>* new_entries) {
  NSUInteger length = 0;
  NSUInteger max_length = MIN(old_entries.count, new_entries.count);
  while (length < max_length && old_entries[length] == new_entries[length]) {
    ++length;
  }
  return length;
}

}  // namespace

ActuationWorklogEntriesDiff ComputeActuationWorklogEntriesDiff(
    NSArray<ActuationWorklogEntry*>* old_entries,
    NSArray<ActuationWorklogEntry*>* new_entries) {
  ActuationWorklogEntriesDiff diff;
  new_entries = new_entries ?: @[];
  NSUInteger old_count = old_entries.count;
  NSUInteger shared_length = SharedPrefixLength(old_entries, new_entries);
  // Resets if entries were removed, or if an old entry other than the last one
  // changed.
  if (new_entries.count < old_count || shared_length + 1 < old_count) {
    diff.reset = true;
    diff.appended = new_entries;
    return diff;
  }

  if (shared_length < old_count) {
    diff.replaced_last = new_entries[shared_length];
  }
  diff.appended = [new_entries
      subarrayWithRange:NSMakeRange(old_count, new_entries.count - old_count)];
  return diff;
}
