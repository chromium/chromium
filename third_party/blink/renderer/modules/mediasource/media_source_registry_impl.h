// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_registry.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

class KURL;

// This singleton lives on the main thread. It allows registration and
// deregistration of MediaSource object URLs from the main thread.
class MediaSourceRegistryImpl final : public MediaSourceRegistry {
 public:
  // Creates the singleton instance.
  static void Init();

  // MediaSourceRegistry override for registering blob URLs referring to the
  // specified media source attachment.
  void RegisterUrl(const KURL&, scoped_refptr<MediaSourceAttachment>) override;

  // UnregisterUrl() unregisters the attachment and removes its entry from
  // |media_sources_| if the serialized URL is present.
  void UnregisterUrl(const KURL&) override;

  // MediaSourceRegistry override that finds |url| in |media_sources_| and
  // returns the corresponding scoped_refptr if found. Otherwise, returns an
  // unset scoped_refptr. |url| must be non-empty.
  scoped_refptr<MediaSourceAttachment> LookupMediaSource(
      const String& url) override;

 private:
  // Construction of this singleton informs MediaSourceAttachment of this
  // singleton, for it to use for lookup, registration, and unregistration.
  MediaSourceRegistryImpl();

  HashMap<String, scoped_refptr<MediaSourceAttachment>> media_sources_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_
