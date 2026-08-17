// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/html/media/media_source_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// static
MediaSourceRegistry* MediaSourceAttachment::registry_ = nullptr;

// static
void MediaSourceAttachment::SetRegistry(MediaSourceRegistry* registry) {
  DCHECK(IsMainThread());
  DCHECK(!registry_);
  registry_ = registry;
}

// static
scoped_refptr<MediaSourceAttachment> MediaSourceAttachment::LookupMediaSource(
    const String& url) {
  // The only expected caller is an HTMLMediaElement on the main thread.
  DCHECK(IsMainThread());

  if (!registry_ || url.empty())
    return nullptr;

  return registry_->LookupMediaSource(url);
}

MediaSourceRegistry& MediaSourceAttachment::Registry() const {
  CHECK(registry_);
  return *registry_;
}

MediaSourceAttachment::MediaSourceAttachment() = default;

MediaSourceAttachment::~MediaSourceAttachment() = default;

}  // namespace blink
