// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_LOCK_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_LOCK_OBSERVER_H_

#include "base/macros.h"

namespace content {

// Observer interface to be notified when frames hold resources.
//
// Methods may be called from any sequence and must therefore be thread-safe.
class LockObserver {
 public:
  LockObserver() = default;
  virtual ~LockObserver() = default;

  // Invoked when the number of Web Locks held by a frame switches between zero
  // and non-zero. There is no guarantee that the frame identified by
  // |render_process_id| and |render_frame_id| still exists when this is called.
  virtual void OnFrameStartsHoldingWebLocks(int render_process_id,
                                            int render_frame_id) = 0;
  virtual void OnFrameStopsHoldingWebLocks(int render_process_id,
                                           int render_frame_id) = 0;

  // Invoked when the number of IndexedDB connections (pending or active) in a
  // frame switches between zero and non-zero. There is no guarantee that the
  // frame identified by |render_process_id| and |render_frame_id| still exists
  // when this is called.
  virtual void OnFrameStartsHoldingIndexedDBConnections(
      int render_process_id,
      int render_frame_id) = 0;
  virtual void OnFrameStopsHoldingIndexedDBConnections(int render_process_id,
                                                       int render_frame_id) = 0;

  LockObserver(const LockObserver&) = delete;
  LockObserver& operator=(const LockObserver&) = delete;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_LOCK_OBSERVER_H_
