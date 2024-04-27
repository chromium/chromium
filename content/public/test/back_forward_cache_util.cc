// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/back_forward_cache_util.h"

#include <map>
#include <set>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/common/features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

namespace content {
namespace {

// TODO(crbug.com/40216768): Remove the default parameters from the
// kBackForwardCache feature and remove the complex parameter merging code.
std::vector<base::test::FeatureRefAndParams>
GetDefaultEnabledBackForwardCacheFeaturesAndParams(
    const bool ignore_outstanding_network_request) {
  std::vector<base::test::FeatureRefAndParams> default_features_and_params;
  if (ignore_outstanding_network_request) {
    default_features_and_params.push_back(
        {features::kBackForwardCache,
         {{// BackForwardCache will not be blocked by outstanding network
           // requests.
           //
           // Example Test Failure (https://crbug.com/1324788):
           // Navigating quickly between cached pages can fail flakily with:
           // CanStorePageNow: <URL> : No: blocklisted features: outstanding
           // network request (others)
           "ignore_outstanding_network_request_for_testing", "true"}}});
  } else {
    default_features_and_params.push_back({features::kBackForwardCache, {{}}});
  }

  default_features_and_params.push_back(
      {features::kBackForwardCacheTimeToLiveControl,
       {{// Sets a very long TTL before expiration (longer than the test
         // timeout), so tests that are expecting deletion don't pass when
         // they shouldn't.
         "time_to_live_in_seconds", "3600" /* 1 hour */}}});
  return default_features_and_params;
}

std::vector<base::test::FeatureRefAndParams> Merge(
    const std::vector<base::test::FeatureRefAndParams>&
        default_features_and_params,
    const std::vector<base::test::FeatureRefAndParams>&
        additional_features_and_params) {
  std::vector<base::test::FeatureRefAndParams> final_features_and_params;

  // TODO(crbug.com/40216768): Consider move the below logic to
  // base/test/scoped_feature_list.h.
  // Go over the additional features/params - if they match a default feature,
  // make a new featureparam with the combined features, otherwise just add the
  // additional feature as is.
  for (auto feature_and_params : additional_features_and_params) {
    auto default_feature_and_param = base::ranges::find(
        default_features_and_params, feature_and_params.feature->name,
        [](const base::test::FeatureRefAndParams default_feature) {
          return default_feature.feature->name;
        });
    if (default_feature_and_param != default_features_and_params.end()) {
      base::FieldTrialParams combined_params;
      combined_params.insert(default_feature_and_param->params.begin(),
                             default_feature_and_param->params.end());
      combined_params.insert(feature_and_params.params.begin(),
                             feature_and_params.params.end());
      final_features_and_params.emplace_back(*feature_and_params.feature,
                                             combined_params);
    } else {
      final_features_and_params.emplace_back(feature_and_params);
    }
  }
  // Add any default features we didn't have additional params for.
  for (auto feature_and_params : default_features_and_params) {
    if (!base::Contains(
            final_features_and_params, feature_and_params.feature->name,
            [](const base::test::FeatureRefAndParams default_feature) {
              return default_feature.feature->name;
            })) {
      final_features_and_params.emplace_back(feature_and_params);
    }
  }

  return final_features_and_params;
}

}  // namespace

class BackForwardCacheDisabledTester::Impl
    : public BackForwardCacheTestDelegate {
 public:
  bool IsDisabledForFrameWithReason(GlobalRenderFrameHostId id,
                                    BackForwardCache::DisabledReason reason) {
    return disable_reasons_[id].count(reason) != 0;
  }

  void OnDisabledForFrameWithReason(
      GlobalRenderFrameHostId id,
      BackForwardCache::DisabledReason reason) override {
    disable_reasons_[id].insert(reason);
  }

 private:
  std::map<GlobalRenderFrameHostId, std::set<BackForwardCache::DisabledReason>>
      disable_reasons_;
};

BackForwardCacheDisabledTester::BackForwardCacheDisabledTester()
    : impl_(std::make_unique<Impl>()) {}

BackForwardCacheDisabledTester::~BackForwardCacheDisabledTester() {}

bool BackForwardCacheDisabledTester::IsDisabledForFrameWithReason(
    int process_id,
    int frame_routing_id,
    BackForwardCache::DisabledReason reason) {
  return impl_->IsDisabledForFrameWithReason(
      GlobalRenderFrameHostId{process_id, frame_routing_id}, reason);
}

void DisableBackForwardCacheForTesting(
    WebContents* web_contents,
    BackForwardCache::DisableForTestingReason reason) {
  // Used by tests. Disables BackForwardCache for a given WebContents.
  web_contents->GetController().GetBackForwardCache().DisableForTesting(reason);
}

std::vector<base::test::FeatureRefAndParams>
GetDefaultEnabledBackForwardCacheFeaturesForTesting(
    const bool ignore_outstanding_network_request) {
  return GetDefaultEnabledBackForwardCacheFeaturesForTesting(
      {}, ignore_outstanding_network_request);
}

std::vector<base::test::FeatureRefAndParams>
GetDefaultEnabledBackForwardCacheFeaturesForTesting(
    const std::vector<base::test::FeatureRefAndParams>&
        additional_features_and_params,
    const bool ignore_outstanding_network_request) {
  auto default_features_and_params =
      GetDefaultEnabledBackForwardCacheFeaturesAndParams(
          ignore_outstanding_network_request);

  return Merge(default_features_and_params, additional_features_and_params);
}

std::vector<base::test::FeatureRefAndParams>
GetDefaultEnabledBackForwardCacheFeaturesForTesting(
    const std::vector<base::test::FeatureRefAndParams>&
        additional_features_and_params,
    const size_t cache_size,
    const size_t foreground_cache_size,
    const bool ignore_outstanding_network_request) {
  auto default_features_and_params =
      GetDefaultEnabledBackForwardCacheFeaturesAndParams(
          ignore_outstanding_network_request);
  default_features_and_params.push_back(
      {kBackForwardCacheSize,
       {{"cache_size", base::NumberToString(cache_size)},
        {"foreground_cache_size",
         base::NumberToString(foreground_cache_size)}}});

  return Merge(default_features_and_params, additional_features_and_params);
}

std::vector<base::test::FeatureRefAndParams>
GetBasicBackForwardCacheFeatureForTesting() {
  return GetBasicBackForwardCacheFeatureForTesting({});
}

std::vector<base::test::FeatureRefAndParams>
GetBasicBackForwardCacheFeatureForTesting(
    const std::vector<base::test::FeatureRefAndParams>&
        additional_features_and_params) {
  std::vector<base::test::FeatureRefAndParams> default_features_and_params = {
      {features::kBackForwardCache, {}}};

  return Merge(default_features_and_params, additional_features_and_params);
}

std::vector<base::test::FeatureRef>
GetDefaultDisabledBackForwardCacheFeaturesForTesting() {
  return GetDefaultDisabledBackForwardCacheFeaturesForTesting({});
}

std::vector<base::test::FeatureRef>
GetDefaultDisabledBackForwardCacheFeaturesForTesting(
    const std::vector<base::test::FeatureRef>& additional_features) {
  // Allows BackForwardCache for all devices regardless of their memory,
  // including lower memory Android devices.
  std::vector<base::test::FeatureRef> final_features = {
      features::kBackForwardCacheMemoryControls};

  for (auto additional_feature : additional_features) {
    if (!base::Contains(final_features, additional_feature->name,
                        [](const base::test::FeatureRef default_feature) {
                          return default_feature->name;
                        })) {
      final_features.emplace_back(additional_feature);
    }
  }
  return final_features;
}

void InitBackForwardCacheFeature(base::test::ScopedFeatureList* feature_list,
                                 bool enable_back_forward_cache) {
  if (enable_back_forward_cache) {
    feature_list->InitWithFeaturesAndParameters(
        GetBasicBackForwardCacheFeatureForTesting(
            {{kBackForwardCacheNoTimeEviction, {}},
             {features::kBackForwardCacheMemoryControls, {}}}),
        {});
  } else {
    feature_list->InitAndDisableFeature(features::kBackForwardCache);
  }
}

}  // namespace content
