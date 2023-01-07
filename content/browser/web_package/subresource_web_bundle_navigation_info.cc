// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/subresource_web_bundle_navigation_info.h"

namespace content {

SubresourceWebBundleNavigationInfo::SubresourceWebBundleNavigationInfo(
    const GURL bundle_url,
    base::UnguessableToken token,
    int32_t render_process_id)
    : bundle_url_(bundle_url),
      token_(token),
      render_process_id_(render_process_id) {}

SubresourceWebBundleNavigationInfo::~SubresourceWebBundleNavigationInfo() =
    default;

std::unique_ptr<SubresourceWebBundleNavigationInfo>
SubresourceWebBundleNavigationInfo::Clone() const {
  return std::make_unique<SubresourceWebBundleNavigationInfo>(
      bundle_url_, token_, render_process_id_);
}

}  // namespace content
