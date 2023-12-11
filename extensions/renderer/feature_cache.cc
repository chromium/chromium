// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/feature_cache.h"

#include "base/command_line.h"
#include "base/containers/map_util.h"
#include "base/ranges/algorithm.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/renderer/dispatcher.h"

namespace extensions {

FeatureCache::ExtensionFeatureData::ExtensionFeatureData() = default;
FeatureCache::ExtensionFeatureData::ExtensionFeatureData(
    const ExtensionFeatureData&) = default;
FeatureCache::ExtensionFeatureData::~ExtensionFeatureData() = default;

FeatureCache::FeatureCache() = default;
FeatureCache::~FeatureCache() = default;

FeatureCache::FeatureNameVector FeatureCache::GetAvailableFeatures(
    mojom::ContextType context_type,
    const Extension* extension,
    const GURL& url,
    const ContextData& context_data) {
  bool is_webui_or_untrusted_webui =
      context_type == mojom::ContextType::kWebUi ||
      context_type == mojom::ContextType::kUntrustedWebUi;
  DCHECK_NE(is_webui_or_untrusted_webui, !!extension)
      << "WebUI contexts shouldn't have extensions.";
  DCHECK_NE(mojom::ContextType::kWebPage, context_type)
      << "FeatureCache shouldn't be used for web contexts.";
  DCHECK_NE(mojom::ContextType::kUnspecified, context_type)
      << "FeatureCache shouldn't be used for unspecified contexts.";

  const ExtensionFeatureData& features = GetFeaturesFromCache(
      context_type, extension, url.DeprecatedGetOriginAsURL(),
      kRendererProfileId, context_data);
  FeatureNameVector names;
  names.reserve(features.available_features.size());
  for (const Feature* feature : features.available_features) {
    // Since we only cache based on extension id and context type, instead of
    // all attributes of a context (like URL), we need to double-check if the
    // feature is actually available to the context. This is still a win, since
    // we only perform this check on the (much smaller) set of features that
    // *may* be available, rather than all known features.
    // TODO(devlin): Optimize this - we should be able to tell if a feature may
    // change based on additional context attributes.
    if (ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
            *feature, extension, context_type, url,
            CheckAliasStatus::NOT_ALLOWED, kRendererProfileId, context_data)) {
      names.push_back(feature->name());
    }
  }
  return names;
}

FeatureCache::FeatureNameVector
FeatureCache::GetDeveloperModeRestrictedFeatures(
    mojom::ContextType context_type,
    const Extension* extension,
    const GURL& url,
    const ContextData& context_data) {
  const ExtensionFeatureData& features = GetFeaturesFromCache(
      context_type, extension, url.DeprecatedGetOriginAsURL(),
      kRendererProfileId, context_data);
  FeatureNameVector names;
  names.reserve(features.dev_mode_restricted_features.size());
  for (const Feature* feature : features.dev_mode_restricted_features) {
    names.push_back(feature->name());
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

void FeatureCache::InvalidateAllExtensions() {
  extension_cache_.clear();
}

const FeatureCache::ExtensionFeatureData& FeatureCache::GetFeaturesFromCache(
    mojom::ContextType context_type,
    const Extension* extension,
    const GURL& origin,
    int context_id,
    const ContextData& context_data) {
  if (context_type == mojom::ContextType::kWebUi ||
      context_type == mojom::ContextType::kUntrustedWebUi) {
    if (auto* data = base::FindOrNull(webui_cache_, origin)) {
      return *data;
    }
    return webui_cache_
        .emplace(origin, CreateCacheEntry(context_type, extension, origin,
                                          context_id, context_data))
        .first->second;
  }

  DCHECK(extension);
  ExtensionCacheMapKey key(extension->id(), context_type);
  if (auto* data = base::FindOrNull(extension_cache_, key)) {
    return *data;
  }
  return extension_cache_
      .emplace(key, CreateCacheEntry(context_type, extension, origin,
                                     context_id, context_data))
      .first->second;
}

FeatureCache::ExtensionFeatureData FeatureCache::CreateCacheEntry(
    mojom::ContextType context_type,
    const Extension* extension,
    const GURL& origin,
    int context_id,
    const ContextData& context_data) {
  ExtensionFeatureData features;
  const FeatureProvider* api_feature_provider =
      FeatureProvider::GetAPIFeatures();
  GURL empty_url;
  // We ignore the URL if this is an extension context in order to maximize
  // cache hits. For WebUI and untrusted WebUI, we key on origin.
  // Note: Currently, we only ever have matches based on origin, so this is
  // okay. If this changes, we'll have to get more creative about our WebUI
  // caching.
  const bool should_use_url =
      (context_type == mojom::ContextType::kWebUi ||
       context_type == mojom::ContextType::kUntrustedWebUi);
  const GURL& url_to_use = should_use_url ? origin : empty_url;
  for (const auto& [name, feature] : api_feature_provider->GetAllFeatures()) {
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
    if (name == "test" && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                              ::switches::kTestType)) {
      continue;
    }

    if (!ExtensionAPI::GetSharedInstance()->IsAnyFeatureAvailableToContext(
            *feature, extension, context_type, url_to_use,
            CheckAliasStatus::NOT_ALLOWED, context_id, context_data)) {
      if (feature
              ->IsAvailableToContextIgnoringDevMode(
                  extension, context_type, url_to_use,
                  Feature::GetCurrentPlatform(), context_id, context_data)
              .is_available()) {
        features.dev_mode_restricted_features.push_back(feature.get());
      }
      continue;
    }

    features.available_features.push_back(feature.get());
  }

  base::ranges::sort(features.dev_mode_restricted_features,
                     base::ranges::less{}, &Feature::name);
  base::ranges::sort(features.available_features, base::ranges::less{},
                     &Feature::name);
  DCHECK(base::ranges::unique(features.dev_mode_restricted_features) ==
         features.dev_mode_restricted_features.end());
  DCHECK(base::ranges::unique(features.available_features) ==
         features.available_features.end());

  return features;
}

}  // namespace extensions
