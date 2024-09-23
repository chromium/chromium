// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_
#define CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/back_forward_cache.h"

namespace content {
class WebContents;

// This is a helper class to check in the tests that back-forward cache
// was disabled for a particular reason.
//
// This class should be created in the beginning of the test and will
// know about all BackForwardCache::DisableForRenderFrameHost which
// happened during its lifetime.
//
// Typical usage pattern:
//
// BackForwardCacheDisabledTester helper;
// NavigateToURL(page_with_feature);
// NavigateToURL(away);
// EXPECT_TRUE/FALSE(helper.IsDisabledForFrameWithReason());

class BackForwardCacheDisabledTester {
 public:
  BackForwardCacheDisabledTester();
  ~BackForwardCacheDisabledTester();

  bool IsDisabledForFrameWithReason(int process_id,
                                    int frame_routing_id,
                                    BackForwardCache::DisabledReason reason);

 private:
  // Impl has to inherit from BackForwardCacheImpl, which is
  // a content/-internal concept, so we can include it only from
  // .cc file.
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Helper function to be used when the tests are interested in covering the
// scenarios when back-forward cache is not used. This is similar to method
// BackForwardCache::DisableForTesting(), but it takes a WebContents instead of
// a BackForwardCache. This method disables BackForwardCache for a given
// WebContents with the reason specified.
//
// Note that it is preferred to make the test work with BackForwardCache when
// feasible, or have a standalone test with BackForwardCache enabled to test
// the functionality when necessary.
void DisableBackForwardCacheForTesting(
    WebContents* web_contents,
    BackForwardCache::DisableForTestingReason reason);

// Returns a vector of default features with parameters to set up the
// BackForwardCache for testing, including enabling the cache, allowing
// outstanding network requests to not block BackForwardCache, setting longer
// cache timeout. Example:
//
//     base::test::ScopedFeatureList feature_list;
//     feature_list.InitWithFeaturesAndParameters(
//         GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
//         GetDefaultDisabledBackForwardCacheFeaturesForTesting());
//
// Set `ignore_outstanding_network_request` to true to avoid flaky behavior when
// navigating quickly between cached pages.
std::vector<base::test::FeatureRefAndParams>
GetDefaultEnabledBackForwardCacheFeaturesForTesting(
    const bool ignore_outstanding_network_request = true);
// Similar to `GetDefaultEnabledBackForwardCacheFeaturesForTesting()` above, but
// `additional_features_and_params` can be passed to specify additional features
// and parameters that will be in the returned vector.
std::vector<base::test::FeatureRefAndParams>
GetDefaultEnabledBackForwardCacheFeaturesForTesting(
    const std::vector<base::test::FeatureRefAndParams>&
        additional_features_and_params,
    const bool ignore_outstanding_network_request = true);
// Similar to `GetDefaultEnabledBackForwardCacheFeaturesForTesting()` above, but
// `additional_features_and_params` can be passed to specify additional features
// and parameters that will be in the returned vector.
// `cache_size` and `foreground_cache_size` can be passed to overwrite the
// corresponding size configs to help testing.
std::vector<base::test::FeatureRefAndParams>
GetDefaultEnabledBackForwardCacheFeaturesForTesting(
    const std::vector<base::test::FeatureRefAndParams>&
        additional_features_and_params,
    const size_t cache_size,
    const size_t foreground_cache_size,
    const bool ignore_outstanding_network_request = true);

// TODO(crbug.com/40216768): Consider remove this group of functions by updating
// their callers to use the above ones.
// Returns a vector to set up the BackForwardCache for testing.
//
// The returned vector only contain a single feature to enable BackForwardCache
// itself but no other features and parameters, unlike
// the defaults for testing from
// `GetDefaultEnabledBackForwardCacheFeaturesForTesting()`.
std::vector<base::test::FeatureRefAndParams>
GetBasicBackForwardCacheFeatureForTesting();
// Similar to `GetBasicBackForwardCacheFeatureForTesting()` above, but
// `additional_features_and_params` specifies additional features and parameters
// that will be in the returned structure.
std::vector<base::test::FeatureRefAndParams>
GetBasicBackForwardCacheFeatureForTesting(
    const std::vector<base::test::FeatureRefAndParams>&
        additional_features_and_params);

// Returns a vector of features to disable by default when testing with the
// BackForwardCache. Example:
//
//     base::test::ScopedFeatureList feature_list;
//     feature_list.InitWithFeaturesAndParameters(
//         GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
//         GetDefaultDisabledBackForwardCacheFeaturesForTesting());
std::vector<base::test::FeatureRef>
GetDefaultDisabledBackForwardCacheFeaturesForTesting();
// Similar to `GetDefaultDisabledBackForwardCacheFeaturesForTesting()` above,
// but `additional_features` can be passed to specify additional features that
// will be in the returned vector.
std::vector<base::test::FeatureRef>
GetDefaultDisabledBackForwardCacheFeaturesForTesting(
    const std::vector<base::test::FeatureRef>& additional_features);

// Initializes BFCache to `enable_back_forward_cache` for `feature_list`.
void InitBackForwardCacheFeature(base::test::ScopedFeatureList* feature_list,
                                 bool enable_back_forward_cache);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BACK_FORWARD_CACHE_UTIL_H_
