// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_registry.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

class KURL;

// This singleton lives on the main thread. It allows registration and
// deregistration of MediaSource objectUrls from both main and dedicated worker
// threads, internally locking to access or update |media_sources_| coherently.
// TODO(crbug.com/878133): Completely remove the ability to use this from
// dedicated worker threads once MediaSourceInWorkersUsingHandle has shipped
// stable.
class MediaSourceRegistryImpl final : public MediaSourceRegistry {
 public:
  // Creates the singleton instance. Must be run on the main thread (expected to
  // be done by modules initialization to ensure it happens early and on the
  // main thread.)
  static void Init();

  // MediaSourceRegistry : URLRegistry overrides for (un)registering blob URLs
  // referring to the specified media source attachment, potentially
  // cross-thread. RegisterURL creates a scoped_refptr to manage the
  // registrable's ref-counted lifetime and puts it in |media_sources_|.  Can be
  // called from either the main thread or a dedicated worker thread.
  // Regardless, must be called on the thread which created the URLRegistrable
  // (the MediaSourceAttachment).
  void RegisterURL(SecurityOrigin*, const KURL&, URLRegistrable*) override
      LOCKS_EXCLUDED(map_lock_);

  // UnregisterURL removes the corresponding scoped_refptr and KURL from
  // |media_sources_| if its KURL was there. Can be called from either the main
  // thread (explicit revocation or automatic revocation on attachment success)
  // or from a worker thread (explicit revocation on worker context or worker
  // context destruction).
  void UnregisterURL(const KURL&) override LOCKS_EXCLUDED(map_lock_);

  // MediaSourceRegistry override that finds |url| in |media_sources_| and
  // returns the corresponding scoped_refptr if found. Otherwise, returns an
  // unset scoped_refptr. |url| must be non-empty. Even with
  // MediaSourceInWorkers feature, this must only be called on the main thread
  // (typically for attachment of MediaSource to an HTMLMediaElement).
  scoped_refptr<MediaSourceAttachment> LookupMediaSource(
      const String& url) override LOCKS_EXCLUDED(map_lock_);

 private:
  // Construction of this singleton informs MediaSourceAttachment of this
  // singleton, for it to use to service URLRegistry interface activities on
  // this registry like lookup, registration and unregistration.
  MediaSourceRegistryImpl();

  mutable base::Lock map_lock_;
  HashMap<String, scoped_refptr<MediaSourceAttachment>> media_sources_
      GUARDED_BY(map_lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_
