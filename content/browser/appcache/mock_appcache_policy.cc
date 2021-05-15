// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/mock_appcache_policy.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"

namespace content {

MockAppCachePolicy::MockAppCachePolicy()
    : can_load_return_value_(true), can_create_return_value_(true) {
}

MockAppCachePolicy::~MockAppCachePolicy() = default;

bool MockAppCachePolicy::CanLoadAppCache(
    const GURL& manifest_url,
    const GURL& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin) {
  requested_manifest_url_ = manifest_url;
  return can_load_return_value_;
}

bool MockAppCachePolicy::CanCreateAppCache(
    const GURL& manifest_url,
    const GURL& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin) {
  requested_manifest_url_ = manifest_url;
  return can_create_return_value_;
}

bool MockAppCachePolicy::IsOriginTrialRequiredForAppCache() {
  // Ignore the force enable override preference here and just check the
  // feature.
  return base::FeatureList::IsEnabled(
      blink::features::kAppCacheRequireOriginTrial);
}

}  // namespace content
