// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/back_forward_cache_util.h"

#include <map>
#include <set>

#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

using base::test::ScopedFeatureList;

namespace content {

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

std::vector<ScopedFeatureList::FeatureAndParams>
DefaultEnabledBackForwardCacheParametersForTests() {
  return DefaultEnabledBackForwardCacheParametersForTests({});
}

std::vector<ScopedFeatureList::FeatureAndParams>
DefaultEnabledBackForwardCacheParametersForTests(
    const std::vector<ScopedFeatureList::FeatureAndParams>& additional_params) {
  // TODO(https://crbug.com/1301867): Remove the default parameters from the
  // kBackForwardCache feature and remove the complex parameter merging code.
  std::vector<ScopedFeatureList::FeatureAndParams> default_features_and_params =
      {{features::kBackForwardCache,
        {{"ignore_outstanding_network_request_for_testing", "true"}}},
       {kBackForwardCacheTimeToLiveControl,
        {{"time_to_live_in_seconds", "3600"}}}};
  std::vector<ScopedFeatureList::FeatureAndParams> final_params;
  // Go over the additional features/params - if they match a default feature,
  // make a new featureparam with the combined features, otherwise just add the
  // additional feature as is.
  for (auto feature_and_params : additional_params) {
    auto default_feature_and_param = std::find_if(
        default_features_and_params.begin(), default_features_and_params.end(),
        [&feature_and_params](
            const ScopedFeatureList::FeatureAndParams default_feature) {
          return default_feature.feature.name ==
                 feature_and_params.feature.name;
        });
    if (default_feature_and_param != default_features_and_params.end()) {
      base::FieldTrialParams combined_params;
      combined_params.insert(default_feature_and_param->params.begin(),
                             default_feature_and_param->params.end());
      combined_params.insert(feature_and_params.params.begin(),
                             feature_and_params.params.end());
      final_params.emplace_back(ScopedFeatureList::FeatureAndParams(
          feature_and_params.feature, combined_params));
    } else {
      final_params.emplace_back(feature_and_params);
    }
  }
  // Add any default features we didn't have additional params for.
  for (auto feature_and_params : default_features_and_params) {
    auto default_param = std::find_if(
        final_params.begin(), final_params.end(),
        [&feature_and_params](
            const ScopedFeatureList::FeatureAndParams default_feature) {
          return default_feature.feature.name ==
                 feature_and_params.feature.name;
        });
    if (default_param == final_params.end()) {
      final_params.emplace_back(feature_and_params);
    }
  }
  return final_params;
}

std::vector<base::Feature> DefaultDisabledBackForwardCacheParametersForTests() {
  return {features::kBackForwardCacheMemoryControls};
}

}  // namespace content
