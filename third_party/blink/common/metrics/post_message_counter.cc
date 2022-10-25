// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/metrics/post_message_counter.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"

namespace {
constexpr size_t kMaxRecordedPostsSize = 20;

enum class PostMessageType {
  kOpaque,
  kFirstPartyToFirstPartySameBucket,
  kFirstPartyToFirstPartyDifferentBucket,
  kFirstPartyToThirdPartyDifferentBucketSameOrigin,
  kFirstPartyToThirdPartyDifferentBucketDifferentOrigin,
  kThirdPartyToFirstPartyDifferentBucketSameOrigin,
  kThirdPartyToFirstPartyDifferentBucketDifferentOrigin,
  kThirdPartyToThirdPartySameBucket,
  kThirdPartyToThirdPartyDifferentBucketSameOrigin,
  kThirdPartyToThirdPartyDifferentBucketDifferentOrigin,
};

PostMessageType GetPostMessageType(
    const blink::StorageKey& source_storage_key,
    const blink::StorageKey& target_storage_key) {
  // We want these storage keys to behave as though storage partitioning is on
  // for convenience.
  const blink::StorageKey source_3psp_key =
      source_storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning();
  const blink::StorageKey target_3psp_key =
      target_storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning();

  if (source_3psp_key.origin().opaque() ||
      source_3psp_key.top_level_site().opaque() ||
      target_3psp_key.origin().opaque() ||
      target_3psp_key.top_level_site().opaque()) {
    // We want to return here if any component of the storage key is opaque.
    return PostMessageType::kOpaque;
  } else if (source_3psp_key.IsFirstPartyContext() &&
             target_3psp_key.IsFirstPartyContext()) {
    // If both storage keys are first party . . .
    if (source_3psp_key == target_3psp_key) {
      // . . . we note if they are identical . . .
      return PostMessageType::kFirstPartyToFirstPartySameBucket;
    } else {
      // . . . or if they are in any way distinct.
      return PostMessageType::kFirstPartyToFirstPartyDifferentBucket;
    }
  } else if (source_3psp_key.IsFirstPartyContext() &&
             target_3psp_key.IsThirdPartyContext()) {
    // If the source is first party and the target is third party . . .
    if (source_3psp_key.origin() == target_3psp_key.origin()) {
      // . . . we note if their origins are identical . . .
      return PostMessageType::kFirstPartyToThirdPartyDifferentBucketSameOrigin;
    } else {
      // . . . or the origins are distinct.
      return PostMessageType::
          kFirstPartyToThirdPartyDifferentBucketDifferentOrigin;
    }
  } else if (source_3psp_key.IsThirdPartyContext() &&
             target_3psp_key.IsFirstPartyContext()) {
    // If the source is third party and the target is first party . . .
    if (source_3psp_key.origin() == target_3psp_key.origin()) {
      // . . . we note if their origins are identical . . .
      return PostMessageType::kThirdPartyToFirstPartyDifferentBucketSameOrigin;
    } else {
      // . . . or the origins are distinct.
      return PostMessageType::
          kThirdPartyToFirstPartyDifferentBucketDifferentOrigin;
    }
  } else if (source_3psp_key.IsThirdPartyContext() &&
             target_3psp_key.IsThirdPartyContext()) {
    // If both storage keys are third party . . .
    if (source_3psp_key == target_3psp_key) {
      // . . . we note if they are identical . . .
      return PostMessageType::kThirdPartyToThirdPartySameBucket;
    } else if (source_3psp_key.origin() == target_3psp_key.origin()) {
      // . . . or if their origins alone are identical . . .
      return PostMessageType::kThirdPartyToThirdPartyDifferentBucketSameOrigin;
    } else {
      // . . . or the origins are distinct.
      return PostMessageType::
          kThirdPartyToThirdPartyDifferentBucketDifferentOrigin;
    }
  }
  NOTREACHED();
  return PostMessageType::kOpaque;
}

void DoLogging(blink::PostMessagePartition partition,
               PostMessageType type,
               ukm::SourceId source_id,
               ukm::SourceId target_id,
               ukm::UkmRecorder* recorder) {
  // Do process related logging.
  switch (partition) {
    case blink::PostMessagePartition::kCrossProcess: {
      ukm::builders::PostMessage_Incoming_Page builder(target_id);
      builder.SetSourcePageSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case blink::PostMessagePartition::kSameProcess: {
      ukm::builders::PostMessage_Incoming_Frame builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
  }

  // Do type related logging.
  switch (type) {
    case PostMessageType::kOpaque: {
      ukm::builders::PostMessage_Incoming_Opaque builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::kFirstPartyToFirstPartySameBucket: {
      ukm::builders::PostMessage_Incoming_FirstPartyToFirstParty_SameBucket
          builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::kFirstPartyToFirstPartyDifferentBucket: {
      ukm::builders::PostMessage_Incoming_FirstPartyToFirstParty_DifferentBucket
          builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::kFirstPartyToThirdPartyDifferentBucketSameOrigin: {
      ukm::builders::
          PostMessage_Incoming_FirstPartyToThirdParty_DifferentBucket_SameOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::
        kFirstPartyToThirdPartyDifferentBucketDifferentOrigin: {
      ukm::builders::
          PostMessage_Incoming_FirstPartyToThirdParty_DifferentBucket_DifferentOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::kThirdPartyToFirstPartyDifferentBucketSameOrigin: {
      ukm::builders::
          PostMessage_Incoming_ThirdPartyToFirstParty_DifferentBucket_SameOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::
        kThirdPartyToFirstPartyDifferentBucketDifferentOrigin: {
      ukm::builders::
          PostMessage_Incoming_ThirdPartyToFirstParty_DifferentBucket_DifferentOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::kThirdPartyToThirdPartySameBucket: {
      ukm::builders::PostMessage_Incoming_ThirdPartyToThirdParty_SameBucket
          builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::kThirdPartyToThirdPartyDifferentBucketSameOrigin: {
      ukm::builders::
          PostMessage_Incoming_ThirdPartyToThirdParty_DifferentBucket_SameOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
    case PostMessageType::
        kThirdPartyToThirdPartyDifferentBucketDifferentOrigin: {
      ukm::builders::
          PostMessage_Incoming_ThirdPartyToThirdParty_DifferentBucket_DifferentOrigin
              builder(target_id);
      builder.SetSourceFrameSourceId(source_id);
      builder.Record(recorder);
      break;
    }
  }
}

bool ShouldSendPostMessage(PostMessageType type) {
  switch (type) {
    case PostMessageType::kOpaque:
    case PostMessageType::kFirstPartyToFirstPartySameBucket:
    case PostMessageType::kFirstPartyToFirstPartyDifferentBucket:
    case PostMessageType::kFirstPartyToThirdPartyDifferentBucketDifferentOrigin:
    case PostMessageType::kThirdPartyToFirstPartyDifferentBucketDifferentOrigin:
    case PostMessageType::kThirdPartyToThirdPartySameBucket:
    case PostMessageType::kThirdPartyToThirdPartyDifferentBucketDifferentOrigin:
      return true;
    case PostMessageType::kFirstPartyToThirdPartyDifferentBucketSameOrigin:
      // This handles two cases: (1) if the general block is enabled or (2) if
      // third party storage partitioning is enabled and the conditional block
      // is enabled. This is inverted as returning false drops the message.
      return !(
          base::FeatureList::IsEnabled(
              blink::features::
                  kPostMessageFirstPartyToThirdPartyDifferentBucketSameOriginBlocked) ||
          (base::FeatureList::IsEnabled(
               blink::features::
                   kPostMessageFirstPartyToThirdPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned) &&
           blink::StorageKey::IsThirdPartyStoragePartitioningEnabled()));
    case PostMessageType::kThirdPartyToFirstPartyDifferentBucketSameOrigin:
      // This handles two cases: (1) if the general block is enabled or (2) if
      // third party storage partitioning is enabled and the conditional block
      // is enabled. This is inverted as returning false drops the message.
      return !(
          base::FeatureList::IsEnabled(
              blink::features::
                  kPostMessageThirdPartyToFirstPartyDifferentBucketSameOriginBlocked) ||
          (base::FeatureList::IsEnabled(
               blink::features::
                   kPostMessageThirdPartyToFirstPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned) &&
           blink::StorageKey::IsThirdPartyStoragePartitioningEnabled()));
    case PostMessageType::kThirdPartyToThirdPartyDifferentBucketSameOrigin:
      // This handles two cases: (1) if the general block is enabled or (2) if
      // third party storage partitioning is enabled and the conditional block
      // is enabled. This is inverted as returning false drops the message.
      return !(
          base::FeatureList::IsEnabled(
              blink::features::
                  kPostMessageThirdPartyToThirdPartyDifferentBucketSameOriginBlocked) ||
          (base::FeatureList::IsEnabled(
               blink::features::
                   kPostMessageThirdPartyToThirdPartyDifferentBucketSameOriginBlockedIfStorageIsPartitioned) &&
           blink::StorageKey::IsThirdPartyStoragePartitioningEnabled()));
  }
  NOTREACHED();
  return false;
}
}  // namespace

namespace blink {
bool PostMessageCounter::RecordMessageAndCheckIfShouldSend(
    ukm::SourceId source_id,
    const StorageKey& source_storage_key,
    ukm::SourceId target_id,
    const StorageKey& target_storage_key,
    ukm::UkmRecorder* recorder) {
  DCHECK_LE(recorded_posts_.size(), kMaxRecordedPostsSize);
  std::pair<ukm::SourceId, ukm::SourceId> new_pair(source_id, target_id);
  PostMessageType type =
      GetPostMessageType(source_storage_key, target_storage_key);
  if (base::Contains(recorded_posts_, new_pair)) {
    return ShouldSendPostMessage(type);
  } else if (recorded_posts_.size() == kMaxRecordedPostsSize) {
    recorded_posts_.pop_back();
  }
  recorded_posts_.push_front(new_pair);
  DoLogging(partition_, type, source_id, target_id, recorder);
  return ShouldSendPostMessage(type);
}
}  // namespace blink
