// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_handle_tracker.h"

#include "content/browser/web_package/web_bundle_handle.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_utils.h"

namespace content {

WebBundleHandleTracker::WebBundleHandleTracker(
    scoped_refptr<WebBundleReader> reader,
    const GURL& target_inner_url)
    : reader_(std::move(reader)), target_inner_url_(target_inner_url) {
  DCHECK(reader_);
}

WebBundleHandleTracker::~WebBundleHandleTracker() = default;

std::unique_ptr<WebBundleHandle>
WebBundleHandleTracker::MaybeCreateWebBundleHandle(const GURL& url,
                                                   int frame_tree_node_id) {
  switch (reader_->source().type()) {
    case WebBundleSource::Type::kTrustedFile:
      if (reader_->HasEntry(url)) {
        return WebBundleHandle::CreateForTrackedNavigation(reader_,
                                                           frame_tree_node_id);
      }
      break;
    case WebBundleSource::Type::kFile:
      if (reader_->HasEntry(url)) {
        return WebBundleHandle::CreateForTrackedNavigation(reader_,
                                                           frame_tree_node_id);
      }
      if (url == web_bundle_utils::GetSynthesizedUrlForWebBundle(
                     reader_->source().url(), target_inner_url_)) {
        // This happens when the page in an untrustable Web Bundle file is
        // reloaded.
        return WebBundleHandle::MaybeCreateForNavigationInfo(
            std::make_unique<WebBundleNavigationInfo>(reader_->source().Clone(),
                                                      target_inner_url_,
                                                      reader_->GetWeakPtr()),
            frame_tree_node_id);
      }
      break;
    case WebBundleSource::Type::kNetwork:
      if (reader_->HasEntry(url) &&
          reader_->source().IsPathRestrictionSatisfied(url)) {
        return WebBundleHandle::CreateForTrackedNavigation(reader_,
                                                           frame_tree_node_id);
      }
      break;
  }
  return nullptr;
}

}  // namespace content
