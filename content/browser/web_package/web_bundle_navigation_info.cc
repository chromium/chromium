// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_navigation_info.h"

#include "content/browser/web_package/web_bundle_source.h"

namespace content {

WebBundleNavigationInfo::WebBundleNavigationInfo(
    std::unique_ptr<WebBundleSource> source,
    const GURL& target_inner_url,
    base::WeakPtr<WebBundleReader> weak_reader)
    : source_(std::move(source)),
      target_inner_url_(target_inner_url),
      weak_reader_(std::move(weak_reader)) {}

WebBundleNavigationInfo::~WebBundleNavigationInfo() = default;

const WebBundleSource& WebBundleNavigationInfo::source() const {
  return *source_.get();
}

const GURL& WebBundleNavigationInfo::target_inner_url() const {
  return target_inner_url_;
}

const base::WeakPtr<WebBundleReader>& WebBundleNavigationInfo::GetReader()
    const {
  return weak_reader_;
}

std::unique_ptr<WebBundleNavigationInfo> WebBundleNavigationInfo::Clone()
    const {
  return std::make_unique<WebBundleNavigationInfo>(
      source_->Clone(), target_inner_url_, weak_reader_);
}

}  // namespace content
