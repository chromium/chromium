// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_IMAGE_RESOURCE_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_IMAGE_RESOURCE_INFO_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_status.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ResourceError;
class ResourceFetcher;
class ResourceResponse;

// Delegate class of ImageResource that encapsulates the interface and data
// visible to ImageResourceContent.
// Do not add new members or new call sites unless really needed.
// TODO(hiroshige): reduce the members of this class to further decouple
// ImageResource and ImageResourceContent.
class CORE_EXPORT ImageResourceInfo : public GarbageCollectedMixin {
 public:
  ~ImageResourceInfo() = default;
  virtual const KURL& Url() const = 0;
  virtual base::TimeTicks LoadResponseEnd() const = 0;
  virtual bool IsSchedulingReload() const = 0;
  virtual const ResourceResponse& GetResponse() const = 0;
  virtual bool ShouldShowPlaceholder() const = 0;
  virtual bool ShouldShowLazyImagePlaceholder() const = 0;
  virtual bool IsCacheValidator() const = 0;
  virtual bool SchedulingReloadOrShouldReloadBrokenPlaceholder() const = 0;
  enum DoesCurrentFrameHaveSingleSecurityOrigin {
    kHasMultipleSecurityOrigin,
    kHasSingleSecurityOrigin
  };
  virtual bool IsAccessAllowed(
      DoesCurrentFrameHaveSingleSecurityOrigin) const = 0;
  virtual bool HasCacheControlNoStoreHeader() const = 0;
  virtual base::Optional<ResourceError> GetResourceError() const = 0;

  // TODO(hiroshige): Remove this once MemoryCache becomes further weaker.
  virtual void SetDecodedSize(size_t) = 0;

  // TODO(hiroshige): Remove these.
  virtual void WillAddClientOrObserver() = 0;
  virtual void DidRemoveClientOrObserver() = 0;

  // TODO(hiroshige): Remove this. crbug.com/666214
  virtual void EmulateLoadStartedForInspector(
      ResourceFetcher*,
      const KURL&,
      const AtomicString& initiator_name) = 0;

  virtual void LoadDeferredImage(ResourceFetcher* fetcher) = 0;

  void Trace(blink::Visitor* visitor) override {}
};

}  // namespace blink

#endif
