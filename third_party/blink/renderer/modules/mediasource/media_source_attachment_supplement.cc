// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasource/media_source_attachment_supplement.h"

#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_tracer.h"

namespace blink {

MediaSourceAttachmentSupplement::MediaSourceAttachmentSupplement(
    MediaSource* media_source)
    : registered_media_source_(media_source) {}

MediaSourceAttachmentSupplement::~MediaSourceAttachmentSupplement() = default;

void MediaSourceAttachmentSupplement::Unregister() {
  DVLOG(1) << __func__ << " this=" << this;

  // The only expected caller is a MediaSourceRegistryImpl on the main thread.
  DCHECK(IsMainThread());

  // Release our strong reference to the MediaSource. Note that revokeObjectURL
  // of the url associated with this attachment could commonly follow this path
  // while the MediaSource (and any attachment to an HTMLMediaElement) may still
  // be alive/active.
  DCHECK(registered_media_source_);
  registered_media_source_ = nullptr;
}

}  // namespace blink
