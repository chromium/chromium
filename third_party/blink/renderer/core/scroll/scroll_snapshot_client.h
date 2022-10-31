// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_SNAPSHOT_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_SNAPSHOT_CLIENT_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class LocalFrame;

class ScrollSnapshotClient : public GarbageCollectedMixin {
 public:
  explicit ScrollSnapshotClient(LocalFrame*);

  // Called exactly once per frame after scroll update and before animation
  // update.
  virtual void UpdateSnapshot() = 0;

  // Called for newly created clients only at layout clean and at most once per
  // frame to handle clients created during style and layout recalc.
  // Returns true if the client state is correct, or false otherwise.
  virtual bool ValidateSnapshot() = 0;

  // Compares the last snapshot with the current state, and returns true if a
  // new animation frame should be schedules due to snapshot difference.
  virtual bool ShouldScheduleNextService() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_SNAPSHOT_CLIENT_H_
