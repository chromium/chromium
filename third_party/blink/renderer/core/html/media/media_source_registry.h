// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_SOURCE_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_SOURCE_REGISTRY_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class KURL;
class MediaSourceAttachment;

// Core interface for registering, unregistering, and looking up
// MediaSourceAttachments. The production implementation lives in
// modules/mediasource.
class CORE_EXPORT MediaSourceRegistry {
  USING_FAST_MALLOC(MediaSourceRegistry);

 public:
  virtual ~MediaSourceRegistry() = default;

  // Registers `attachment` under the given URL and retains its reference until
  // the URL is unregistered.
  virtual void RegisterUrl(const KURL& url,
                           scoped_refptr<MediaSourceAttachment> attachment) = 0;
  virtual void UnregisterUrl(const KURL& url) = 0;

  // Finds the attachment, if any, registered with |url| in the
  // MediaSourceRegistry implementation. |url| must be non-empty. If such an
  // active registration for |url| is not found, returns an unset
  // scoped_refptr<MediaSourceAttachment>.
  virtual scoped_refptr<MediaSourceAttachment> LookupMediaSource(
      const String& url) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_SOURCE_REGISTRY_H_
