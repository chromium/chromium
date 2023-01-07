// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_POST_MESSAGE_COUNTER_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_POST_MESSAGE_COUNTER_H_

#include "base/containers/circular_deque.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace blink {

enum class PostMessagePartition { kSameProcess, kCrossProcess };

// This class is used to bump UKM counters when postMessage is called to
// communicate cross-frame. It tracks the past 20 source/target pairs recorded
// to deduplicate counter bumps and prevent high load. This class is part of an
// effort to track and reduce cross-origin communication.
// TODO(crbug.com/1159586): Remove when no longer needed.
class BLINK_COMMON_EXPORT PostMessageCounter {
 public:
  explicit PostMessageCounter(PostMessagePartition partition)
      : partition_(partition) {}
  ~PostMessageCounter() = default;

  // This function not only bumps postMessage counters, but it returns true if
  // the postMessage itself should be sent. The postMessage might be gated if
  // the storage keys are same-origin but different-partition.
  bool RecordMessageAndCheckIfShouldSend(ukm::SourceId source_id,
                                         const StorageKey& source_storage_key,
                                         ukm::SourceId target_id,
                                         const StorageKey& target_storage_key,
                                         ukm::UkmRecorder* recorder);

 private:
  PostMessagePartition partition_;
  base::circular_deque<std::pair<ukm::SourceId, ukm::SourceId>> recorded_posts_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_METRICS_POST_MESSAGE_COUNTER_H_
