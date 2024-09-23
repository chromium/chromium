// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPABILITIES_PENDING_OPERATIONS_H_
#define MEDIA_CAPABILITIES_PENDING_OPERATIONS_H_

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT PendingOperations {
 public:
  using Id = int;

  // Helper to report timing information for DB operations, including when they
  // hang indefinitely.
  class PendingOperation {
   public:
    PendingOperation(
        std::string uma_str,
        std::unique_ptr<base::CancelableOnceClosure> timeout_closure);
    // Records task timing UMA if it hasn't already timed out.
    virtual ~PendingOperation();

    // Copies disallowed. Incompatible with move-only members and UMA logging in
    // the destructor.
    PendingOperation(const PendingOperation&) = delete;
    PendingOperation& operator=(const PendingOperation&) = delete;

    void UmaHistogramOpTime(base::TimeDelta duration);

    // Trigger UMA recording for timeout.
    void OnTimeout();

   private:
    friend class VideoDecodeStatsDBImplTest;
    friend class WebrtcVideoStatsDBImplTest;
    const std::string uma_str_;
    std::unique_ptr<base::CancelableOnceClosure> timeout_closure_;
    const base::TimeTicks start_ticks_;
  };

  explicit PendingOperations(std::string uma_prefix);
  ~PendingOperations();

  // Creates a PendingOperation using `uma_str` and adds it to `pending_ops_`
  // map. Returns Id for newly started operation. Callers must later call
  // Complete() with this id to destroy the PendingOperation and finalize timing
  // UMA.
  Id Start(std::string_view uma_str);

  // Removes PendingOperation from `pending_ops_` using `op_id_` as a key. This
  // destroys the object and triggers timing UMA.
  void Complete(Id op_id);

  // Unified handler for timeouts of pending DB operations. PendingOperation
  // will be notified that it timed out (to trigger timing UMA) and removed from
  // `pending_ops_`.
  void OnTimeout(Id id);

  const base::flat_map<Id, std::unique_ptr<PendingOperation>>&
  get_pending_ops_for_test() const {
    return pending_ops_;
  }

 private:
  // UMA prefix that is used for pending operations histograms.
  const std::string uma_prefix_;

  // Next Id for use in `pending_ops_` map. See Start().
  Id next_op_id_ = 0;

  // Map of operation id -> outstanding PendingOperations.
  base::flat_map<Id, std::unique_ptr<PendingOperation>> pending_ops_;

  // Ensures all access to class members come on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PendingOperations> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPABILITIES_PENDING_OPERATIONS_H_
