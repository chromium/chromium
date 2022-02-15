// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/metrics/post_message_counter.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {
constexpr size_t kMaxRecordedPostsSize = 20;
}  // namespace

namespace blink {

void PostMessageCounter::RecordMessage(ukm::SourceId source,
                                       ukm::SourceId target,
                                       ukm::UkmRecorder* recorder) {
  DCHECK_LE(recorded_posts_.size(), kMaxRecordedPostsSize);
  std::pair<ukm::SourceId, ukm::SourceId> new_pair(source, target);

  if (base::Contains(recorded_posts_, new_pair))
    return;
  if (recorded_posts_.size() == kMaxRecordedPostsSize)
    recorded_posts_.pop_back();
  recorded_posts_.push_front(new_pair);

  switch (partition_) {
    case PostMessagePartition::kCrossProcess: {
      ukm::builders::PostMessage_Incoming_Page builder(target);
      if (source != ukm::kInvalidSourceId)
        builder.SetSourcePageSourceId(source);
      builder.Record(recorder);
      break;
    }
    case PostMessagePartition::kSameProcess: {
      ukm::builders::PostMessage_Incoming_Frame builder(target);
      if (source != ukm::kInvalidSourceId)
        builder.SetSourceFrameSourceId(source);
      builder.Record(recorder);
      break;
    }
  }
}

}  // namespace blink
