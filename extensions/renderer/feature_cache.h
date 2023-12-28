// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_FEATURE_CACHE_H_
#define EXTENSIONS_RENDERER_FEATURE_CACHE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "url/gurl.h"

namespace extensions {
class Extension;

// Caches features available to different extensions in different context types,
// and returns features available to a given context. Note: currently, this is
// only used for non-webpage contexts.
// TODO(devlin): Use it for all context types?
// Note: This could actually go in extensions/common/, if there was any need for
// it browser-side.
class FeatureCache {
 public:
  using FeatureNameVector = std::vector<std::string>;

  FeatureCache();

  FeatureCache(const FeatureCache&) = delete;
  FeatureCache& operator=(const FeatureCache&) = delete;

  ~FeatureCache();

  // Returns the names of features available to the given set of |context_type|,
  // |extension|, and |url| in a lexicographically sorted vector.
  // Note: these contexts should be valid, so WebUI contexts should have no
  // extensions, extension should be non-null for extension contexts, etc.
  FeatureNameVector GetAvailableFeatures(mojom::ContextType context_type,
                                         const Extension* extension,
                                         const GURL& url,
                                         const ContextData& context_data);

  // Returns the names of features restricted to developer mode in a
  // lexicographically sorted vector.
  FeatureNameVector GetDeveloperModeRestrictedFeatures(
      mojom::ContextType context_type,
      const Extension* extension,
      const GURL& url,
      const ContextData& context_data);

  // Invalidates the cache for the specified extension.
  void InvalidateExtension(const ExtensionId& extension_id);

  // Invalidates the cache for all extensions.
  void InvalidateAllExtensions();

 private:
  using FeatureVector = std::vector<raw_ptr<const Feature, VectorExperimental>>;
  struct ExtensionFeatureData {
   public:
    ExtensionFeatureData();
    ExtensionFeatureData(const ExtensionFeatureData&);
    ~ExtensionFeatureData();

    // Features that are restricted to developer mode.
    FeatureVector dev_mode_restricted_features;
    // Available features that are not restricted to developer mode.
    FeatureVector available_features;
  };

  // Note: We use a key of ExtensionId, mojom::ContextType to maximize cache
  // hits. Unfortunately, this won't always be perfectly accurate, since some
  // features may have other context-dependent restrictions (such as URLs), but
  // caching by extension id + context + url would result in significantly fewer
  // hits.
  using ExtensionCacheMapKey = std::pair<ExtensionId, mojom::ContextType>;
  using ExtensionCacheMap =
      std::map<ExtensionCacheMapKey, ExtensionFeatureData>;

  // Cache by origin.
  using WebUICacheMap = std::map<GURL, ExtensionFeatureData>;

  // Returns the features available to the given context from the cache,
  // creating a new entry if one doesn't exist.
  const ExtensionFeatureData& GetFeaturesFromCache(
      mojom::ContextType context_type,
      const Extension* extension,
      const GURL& origin,
      int context_id,
      const ContextData& context_data);

  // Creates ExtensionFeatureData to be entered into a cache for the specified
  // context data.
  ExtensionFeatureData CreateCacheEntry(mojom::ContextType context_type,
                                        const Extension* extension,
                                        const GURL& origin,
                                        int context_id,
                                        const ContextData& context_data);

  // The cache of extension-related contexts. These may be invalidated, since
  // extension permissions change.
  ExtensionCacheMap extension_cache_;

  // The cache of WebUI-related features. These shouldn't need to be
  // invalidated (since WebUI permissions don't change), and are cached by
  // origin. These covers chrome:// and chrome-untrusted:// URLs.
  WebUICacheMap webui_cache_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_FEATURE_CACHE_H_
