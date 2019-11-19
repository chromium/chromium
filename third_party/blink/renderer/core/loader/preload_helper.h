// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

class AlternateSignedExchangeResourceInfo;
class Document;
class LocalFrame;
class SingleModuleClient;
struct LinkLoadParameters;

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
      const base::Optional<ViewportDescription>&,
      std::unique_ptr<AlternateSignedExchangeResourceInfo>,
      base::Optional<base::UnguessableToken>);
  static Resource* StartPreload(ResourceType,
                                FetchParameters&,
                                ResourceFetcher*);

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
  static Resource* PrefetchIfNeeded(const LinkLoadParameters&, Document&);
  static Resource* PreloadIfNeeded(const LinkLoadParameters&,
                                   Document&,
                                   const KURL& base_url,
                                   LinkCaller,
                                   const base::Optional<ViewportDescription>&,
                                   ParserDisposition);
  static void ModulePreloadIfNeeded(const LinkLoadParameters&,
                                    Document&,
                                    const base::Optional<ViewportDescription>&,
                                    SingleModuleClient*);

  static base::Optional<ResourceType> GetResourceTypeFromAsAttribute(
      const String& as);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_
