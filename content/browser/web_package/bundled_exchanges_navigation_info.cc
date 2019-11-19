// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_navigation_info.h"

#include "content/browser/web_package/bundled_exchanges_source.h"

namespace content {

BundledExchangesNavigationInfo::BundledExchangesNavigationInfo(
    std::unique_ptr<BundledExchangesSource> source,
    const GURL& target_inner_url)
    : source_(std::move(source)), target_inner_url_(target_inner_url) {}

BundledExchangesNavigationInfo::~BundledExchangesNavigationInfo() = default;

const BundledExchangesSource& BundledExchangesNavigationInfo::source() const {
  return *source_.get();
}

const GURL& BundledExchangesNavigationInfo::target_inner_url() const {
  return target_inner_url_;
}

std::unique_ptr<BundledExchangesNavigationInfo>
BundledExchangesNavigationInfo::Clone() const {
  return std::make_unique<BundledExchangesNavigationInfo>(source_->Clone(),
                                                          target_inner_url_);
}

}  // namespace content
