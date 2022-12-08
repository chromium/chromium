// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

class AlternateSignedExchangeResourceInfo;
class Document;
class PendingLinkPreload;
class LocalFrame;
struct LinkLoadParameters;
struct ViewportDescription;

// PreloadHelper is a helper class for preload, module preload, prefetch,
// DNS prefetch, and preconnect triggered by <link> elements and "Link" HTTP
// response headers.
class PreloadHelper final {
  STATIC_ONLY(PreloadHelper);

 public:
  enum CanLoadResources {
    kOnlyLoadResources,
    kDoNotLoadResources,
    kLoadResourcesAndPreconnect
  };

  // Media links cannot be preloaded until the first chunk is parsed. The rest
  // can be preloaded at commit time.
  enum MediaPreloadPolicy { kLoadAll, kOnlyLoadNonMedia, kOnlyLoadMedia };

  static void LoadLinksFromHeader(
      const String& header_value,
      const KURL& base_url,
      LocalFrame&,
      Document*,  // can be nullptr
      CanLoadResources,
      MediaPreloadPolicy,
      const ViewportDescription*,  // can be nullptr
      std::unique_ptr<AlternateSignedExchangeResourceInfo>,
      const base::UnguessableToken* /* can be nullptr */);
  static Resource* StartPreload(ResourceType, FetchParameters&, Document&);

  // Currently only used for UseCounter.
  enum LinkCaller {
    kLinkCalledFromHeader,
    kLinkCalledFromMarkup,
  };

  static void DnsPrefetchIfNeeded(const LinkLoadParameters&,
                                  Document*,
                                  LocalFrame*,
                                  LinkCaller);
  static void PreconnectIfNeeded(const LinkLoadParameters&,
                                 Document*,
                                 LocalFrame*,
                                 LinkCaller);
  static void PrefetchIfNeeded(const LinkLoadParameters&,
                               Document&,
                               PendingLinkPreload*);
  static void PreloadIfNeeded(const LinkLoadParameters&,
                              Document&,
                              const KURL& base_url,
                              LinkCaller,
                              const ViewportDescription*,
                              ParserDisposition,
                              PendingLinkPreload*);
  static void ModulePreloadIfNeeded(const LinkLoadParameters&,
                                    Document&,
                                    const ViewportDescription*,
                                    PendingLinkPreload*);

  static absl::optional<ResourceType> GetResourceTypeFromAsAttribute(
      const String& as);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_
