// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_set.h"

#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

MediaStreamSet* MediaStreamSet::Create(
    ExecutionContext* context,
    const MediaStreamDescriptorVector& stream_descriptors,
    MediaStreamSetInitializedCallback callback) {
  return MakeGarbageCollected<MediaStreamSet>(context, stream_descriptors,
                                              std::move(callback));
}

MediaStreamSet::MediaStreamSet(
    ExecutionContext* context,
    const MediaStreamDescriptorVector& stream_descriptors,
    MediaStreamSetInitializedCallback callback)
    : ExecutionContextClient(context),
      media_streams_to_initialize_count_(stream_descriptors.size()),
      media_streams_initialized_callback_(std::move(callback)) {
  if (!stream_descriptors.IsEmpty()) {
    for (MediaStreamDescriptor* descriptor : stream_descriptors) {
      MediaStream::Create(context, descriptor, /*track=*/nullptr,
                          WTF::Bind(&MediaStreamSet::OnMediaStreamInitialized,
                                    WrapPersistent(this)));
    }
  } else {
    std::move(media_streams_initialized_callback_)
        .Run(initialized_media_streams_);
  }
}

// TODO(crbug.com/1300883): Clean up other streams if one stream capture
// results in an error. This is only required for getDisplayMediaSet.
// Currently existing functionality generates only one stream which is not
// affected by this change.
void MediaStreamSet::OnMediaStreamInitialized(
    MediaStream* initialized_media_stream) {
  DCHECK_LT(initialized_media_streams_.size(),
            media_streams_to_initialize_count_);
  initialized_media_streams_.push_back(initialized_media_stream);
  if (initialized_media_streams_.size() == media_streams_to_initialize_count_) {
    std::move(std::move(media_streams_initialized_callback_))
        .Run(initialized_media_streams_);
  }
}

void MediaStreamSet::Trace(Visitor* visitor) const {
  visitor->Trace(initialized_media_streams_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
