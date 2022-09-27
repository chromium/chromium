// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"

#include "third_party/blink/renderer/core/html/media/media_source_registry.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// static
URLRegistry* MediaSourceAttachment::registry_ = nullptr;

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

  // This cast is safe because the only setter of |registry_| is SetRegistry().
  MediaSourceRegistry* ms_registry =
      static_cast<MediaSourceRegistry*>(registry_);

  return ms_registry->LookupMediaSource(url);
}

MediaSourceAttachment::MediaSourceAttachment() = default;

MediaSourceAttachment::~MediaSourceAttachment() = default;

}  // namespace blink
