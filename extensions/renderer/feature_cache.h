// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_FEATURE_CACHE_H_
#define EXTENSIONS_RENDERER_FEATURE_CACHE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
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
  FeatureNameVector GetAvailableFeatures(Feature::Context context_type,
                                         const Extension* extension,
                                         const GURL& url);

  // Invalidates the cache for the specified extension.
  void InvalidateExtension(const ExtensionId& extension_id);

 private:
  using FeatureVector = std::vector<const Feature*>;
  // Note: We use a key of ExtensionId, Feature::Context to maximize cache hits.
  // Unfortunately, this won't always be perfectly accurate, since some features
  // may have other context-dependent restrictions (such as URLs), but caching
  // by extension id + context + url would result in significantly fewer hits.
  using ExtensionCacheMapKey = std::pair<ExtensionId, Feature::Context>;
  using ExtensionCacheMap = std::map<ExtensionCacheMapKey, FeatureVector>;

  // Cache by origin.
  using WebUICacheMap = std::map<GURL, FeatureVector>;

  // Returns the features available to the given context from the cache,
  // creating a new entry if one doesn't exist.
  const FeatureVector& GetFeaturesFromCache(Feature::Context context_type,
                                            const Extension* extension,
                                            const GURL& origin);

  // Creates a FeatureVector to be entered into a cache for the specified
  // context data.
  FeatureVector CreateCacheEntry(Feature::Context context_type,
                                 const Extension* extension,
                                 const GURL& origin);

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
