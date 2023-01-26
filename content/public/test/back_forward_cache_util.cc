// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/back_forward_cache_util.h"

#include <map>
#include <set>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

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

std::vector<base::test::FeatureRefAndParams>
DefaultEnabledBackForwardCacheParametersForTests() {
  return DefaultEnabledBackForwardCacheParametersForTests({});
}

std::vector<base::test::FeatureRefAndParams>
DefaultEnabledBackForwardCacheParametersForTests(
    const std::vector<base::test::FeatureRefAndParams>& additional_params) {
  // TODO(https://crbug.com/1301867): Remove the default parameters from the
  // kBackForwardCache feature and remove the complex parameter merging code.
  std::vector<base::test::FeatureRefAndParams> default_features_and_params = {
      {features::kBackForwardCache,
       {{"ignore_outstanding_network_request_for_testing", "true"}}},
      {features::kBackForwardCacheTimeToLiveControl,
       {{"time_to_live_in_seconds", "3600"}}}};
  std::vector<base::test::FeatureRefAndParams> final_params;
  // Go over the additional features/params - if they match a default feature,
  // make a new featureparam with the combined features, otherwise just add the
  // additional feature as is.
  for (auto feature_and_params : additional_params) {
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
      final_params.emplace_back(base::test::FeatureRefAndParams(
          *feature_and_params.feature, combined_params));
    } else {
      final_params.emplace_back(feature_and_params);
    }
  }
  // Add any default features we didn't have additional params for.
  for (auto feature_and_params : default_features_and_params) {
    if (!base::Contains(
            final_params, feature_and_params.feature->name,
            [](const base::test::FeatureRefAndParams default_feature) {
              return default_feature.feature->name;
            })) {
      final_params.emplace_back(feature_and_params);
    }
  }
  return final_params;
}

std::vector<base::test::FeatureRef>
DefaultDisabledBackForwardCacheParametersForTests() {
  return {features::kBackForwardCacheMemoryControls};
}

}  // namespace content
