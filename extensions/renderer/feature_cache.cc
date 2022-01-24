// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/feature_cache.h"

#include <algorithm>

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature_provider.h"

namespace extensions {

FeatureCache::FeatureCache() {}
FeatureCache::~FeatureCache() = default;

FeatureCache::FeatureNameVector FeatureCache::GetAvailableFeatures(
    Feature::Context context_type,
    const Extension* extension,
    const GURL& url) {
  bool is_webui_or_untrusted_webui =
      context_type == Feature::WEBUI_CONTEXT ||
      context_type == Feature::WEBUI_UNTRUSTED_CONTEXT;
  DCHECK_NE(is_webui_or_untrusted_webui, !!extension)
      << "WebUI contexts shouldn't have extensions.";
  DCHECK_NE(Feature::WEB_PAGE_CONTEXT, context_type)
      << "FeatureCache shouldn't be used for web contexts.";
  DCHECK_NE(Feature::UNSPECIFIED_CONTEXT, context_type)
      << "FeatureCache shouldn't be used for unspecified contexts.";

  const FeatureVector& features = GetFeaturesFromCache(
      context_type, extension, url.DeprecatedGetOriginAsURL());
  FeatureNameVector names;
  names.reserve(features.size());
  for (const Feature* feature : features) {
    // Since we only cache based on extension id and context type, instead of
    // all attributes of a context (like URL), we need to double-check if the
    // feature is actually available to the context. This is still a win, since
    // we only perform this check on the (much smaller) set of features that
    // *may* be available, rather than all known features.
    // TODO(devlin): Optimize this - we should be able to tell if a feature may
    // change based on additional context attributes.
    if (ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
            *feature, extension, context_type, url,
            CheckAliasStatus::NOT_ALLOWED)) {
      names.push_back(feature->name());
    }
  }
  return names;
}

void FeatureCache::InvalidateExtension(const ExtensionId& extension_id) {
  for (auto iter = extension_cache_.begin(); iter != extension_cache_.end();) {
    if (iter->first.first == extension_id)
      iter = extension_cache_.erase(iter);
    else
      ++iter;
  }
}

const FeatureCache::FeatureVector& FeatureCache::GetFeaturesFromCache(
    Feature::Context context_type,
    const Extension* extension,
    const GURL& origin) {
  if (context_type == Feature::WEBUI_CONTEXT ||
      context_type == Feature::WEBUI_UNTRUSTED_CONTEXT) {
    auto iter = webui_cache_.find(origin);
    if (iter != webui_cache_.end())
      return iter->second;
    return webui_cache_
        .emplace(origin, CreateCacheEntry(context_type, extension, origin))
        .first->second;
  }

  DCHECK(extension);
  ExtensionCacheMapKey key(extension->id(), context_type);
  auto iter = extension_cache_.find(key);
  if (iter != extension_cache_.end())
    return iter->second;
  return extension_cache_
      .emplace(key, CreateCacheEntry(context_type, extension, origin))
      .first->second;
}

FeatureCache::FeatureVector FeatureCache::CreateCacheEntry(
    Feature::Context context_type,
    const Extension* extension,
    const GURL& origin) {
  FeatureVector features;
  const FeatureProvider* api_feature_provider =
      FeatureProvider::GetAPIFeatures();
  GURL empty_url;
  // We ignore the URL if this is an extension context in order to maximize
  // cache hits. For WebUI and untrusted WebUI, we key on origin.
  // Note: Currently, we only ever have matches based on origin, so this is
  // okay. If this changes, we'll have to get more creative about our WebUI
  // caching.
  const bool should_use_url =
      (context_type == Feature::WEBUI_CONTEXT ||
       context_type == Feature::WEBUI_UNTRUSTED_CONTEXT);
  const GURL& url_to_use = should_use_url ? origin : empty_url;
  for (const auto& map_entry : api_feature_provider->GetAllFeatures()) {
    const Feature* feature = map_entry.second.get();
    // Exclude internal APIs.
    if (feature->IsInternal())
      continue;

    // Exclude child features (like events or specific functions).
    // TODO(devlin): Optimize this - instead of skipping child features and then
    // checking IsAnyFeatureAvailableToContext() (which checks child features),
    // we should just check all features directly.
    if (api_feature_provider->GetParent(*feature) != nullptr)
      continue;

    // Skip chrome.test if this isn't a test.
    if (map_entry.first == "test" &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kTestType)) {
      continue;
    }

    if (!ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
            *feature, extension, context_type, url_to_use,
            CheckAliasStatus::NOT_ALLOWED)) {
      continue;
    }

    features.push_back(feature);
  }

  std::sort(
      features.begin(), features.end(),
      [](const Feature* a, const Feature* b) { return a->name() < b->name(); });
  DCHECK(std::unique(features.begin(), features.end()) == features.end());

  return features;
}

}  // namespace extensions
