// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace reading_list {

ReadingListUIDistillationStatus UIStatusFromModelStatus(
    ReadingListEntry::DistillationState distillation_state) {
  switch (distillation_state) {
    case ReadingListEntry::WILL_RETRY:
    case ReadingListEntry::PROCESSING:
    case ReadingListEntry::WAITING:
      return ReadingListUIDistillationStatusPending;
    case ReadingListEntry::PROCESSED:
      return ReadingListUIDistillationStatusSuccess;
    case ReadingListEntry::DISTILLATION_ERROR:
      return ReadingListUIDistillationStatusFailure;
  }
}

}  // namespace reading_list
