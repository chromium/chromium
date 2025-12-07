// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_util.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"

namespace {

// Returns the name of the histogram for `operation`.
const char* OperationHistogramName(SnapshotOperation operation) {
  switch (operation) {
    case SnapshotOperation::kUpdateSnapshot:
      return "IOS.Snapshots.UpdateSnapshotTime";

    case SnapshotOperation::kRetrieveColorSnapshot:
      return "IOS.Snapshots.RetrieveColorSnapshotTime";

    case SnapshotOperation::kRetrieveGreyscaleSnapshot:
      return "IOS.Snapshots.RetrieveGreySnapshotTime";
  }

  NOTREACHED();
}

// Records elapsed time since `start_time` for `operation` returning `image`.
UIImage* RecordElapsedTime(SnapshotOperation operation,
                           base::TimeTicks start_time,
                           UIImage* image) {
  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes(OperationHistogramName(operation), elapsed_time);
  return image;
}

}  // namespace

SnapshotRetrievedBlock BlockRecordingElapsedTime(SnapshotOperation operation,
                                                 SnapshotRetrievedBlock block) {
  base::RepeatingCallback<void(UIImage*)> callback = base::DoNothing();
  if (block) {
    // Ensure `block` is only invoked once, even it the callback is called
    // more than once. This can happen when capturing a snapshot.
    __block SnapshotRetrievedBlock captured_block = block;
    callback = base::BindRepeating(^(UIImage* image) {
      if (captured_block) {
        captured_block(image);
        captured_block = nil;
      }
    });
  }

  return base::CallbackToBlock(
      base::BindRepeating(&RecordElapsedTime, operation, base::TimeTicks::Now())
          .Then(callback));
}
