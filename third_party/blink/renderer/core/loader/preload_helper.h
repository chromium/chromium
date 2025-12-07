// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_

#include <optional>

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
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(LoadLinksFromHeaderMode)
  enum class LoadLinksFromHeaderMode {
    kDocumentBeforeCommit = 0,
    kDocumentAfterCommitWithoutViewport = 1,
    kDocumentAfterCommitWithViewport = 2,
    kDocumentAfterLoadCompleted = 3,
    kSubresourceFromMemoryCache = 4,
    kSubresourceNotFromMemoryCache = 5,
    kMaxValue = kSubresourceNotFromMemoryCache,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:LoadLinksFromHeaderMode)

  // Distinguishes whether a preloading request is initiated by a resource from
  // 'Same-' or 'Cross-' origin from the document's origin, and whether the
  // request refers to the resource from 'Same-' or 'Cross-' origin from the
  // documents's one as well.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(OriginStatusOnSubresource)
  enum class OriginStatusOnSubresource {
    kFromSameOriginToSameOrigin = 0,
    kFromSameOriginToCrossOrigin = 1,
    kFromCrossOriginToSameOrigin = 2,
    kFromCrossOriginToCrossOrigin = 3,
    kMaxValue = kFromCrossOriginToCrossOrigin,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:OriginStatusOnSubresource)

  static void LoadLinksFromHeader(
      const String& header_value,
      const KURL& base_url,
      LocalFrame&,
      Document*,  // can be nullptr
      LoadLinksFromHeaderMode,
      const ViewportDescription*,  // can be nullptr
      std::unique_ptr<AlternateSignedExchangeResourceInfo>,
      const base::UnguessableToken*
          recursive_prefetch_token /* can be nullptr */);
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
  static void FetchCompressionDictionaryIfNeeded(const LinkLoadParameters&,
                                                 Document&,
                                                 PendingLinkPreload*);

  static std::optional<ResourceType> GetResourceTypeFromAsAttribute(
      const String& as);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRELOAD_HELPER_H_
