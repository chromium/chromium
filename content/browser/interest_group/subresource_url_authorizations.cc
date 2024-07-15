// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/subresource_url_authorizations.h"

#include <map>
#include <tuple>
#include <vector>

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"

namespace content {

SubresourceUrlAuthorizations::SubresourceUrlAuthorizations() = default;

SubresourceUrlAuthorizations::~SubresourceUrlAuthorizations() {
  DCHECK_EQ(0u, subresource_urls_per_handle_.size());
  DCHECK_EQ(0u, authorized_subresource_urls_.size());
}

void SubresourceUrlAuthorizations::AuthorizeSubresourceUrls(
    const AuctionWorkletManager::WorkletHandle* worklet_handle,
    const std::vector<SubresourceUrlBuilder::BundleSubresourceInfo>&
        authorized_subresource_urls) {
  DCHECK_EQ(0u, subresource_urls_per_handle_.count(worklet_handle));
  std::vector<GURL> subresource_urls;
  for (const SubresourceUrlBuilder::BundleSubresourceInfo& full_info :
       authorized_subresource_urls) {
    auto it = authorized_subresource_urls_.find(full_info.subresource_url);
    if (it == authorized_subresource_urls_.end()) {
      bool inserted;
      std::tie(it, inserted) = authorized_subresource_urls_.emplace(
          full_info.subresource_url, full_info);
      DCHECK(inserted);
    }
    // For a given frame on a page, a subresource bundle resource can only be
    // declared once at any given time -- the token and bundle URL for the
    // subresource URL are unique. SubresourceUrlAuthorizations is per-proxy,
    // which is per frame, so as long as the page doesn't alter the <script
    // type="webbundle"> for the subresource, the bundle_url should be the same.
    //
    // TODO(crbug.com/40876285): Once we have shared-ownership handles to bundle
    // subresources, allow sites to alter their <script> tags after calling
    // runAdAuction().
    //
    // TODO(crbug.com/40223695): If the tokens match, but the bundle URLs don't,
    // report a bad mojo message from the renderer.
    ++it->second.count;

    subresource_urls.push_back(full_info.subresource_url);
  }
  auto [unused_it, success] = subresource_urls_per_handle_.emplace(
      worklet_handle, std::move(subresource_urls));
  DCHECK(success);
}

void SubresourceUrlAuthorizations::OnWorkletHandleDestruction(
    const AuctionWorkletManager::WorkletHandle* worklet_handle) {
  auto per_handle_it = subresource_urls_per_handle_.find(worklet_handle);
  CHECK(per_handle_it != subresource_urls_per_handle_.end(),
        base::NotFatalUntil::M130);
  for (const GURL& subresource_url : per_handle_it->second) {
    auto authorized_urls_it =
        authorized_subresource_urls_.find(subresource_url);
    CHECK(authorized_urls_it != authorized_subresource_urls_.end(),
          base::NotFatalUntil::M130);
    if (--authorized_urls_it->second.count <= 0)
      authorized_subresource_urls_.erase(authorized_urls_it);
  }
  subresource_urls_per_handle_.erase(worklet_handle);
}

const SubresourceUrlBuilder::BundleSubresourceInfo*
SubresourceUrlAuthorizations::GetAuthorizationInfo(
    const GURL& subresource_url) const {
  auto it = authorized_subresource_urls_.find(subresource_url);
  if (it == authorized_subresource_urls_.end())
    return nullptr;
  return &it->second.full_info;
}

bool SubresourceUrlAuthorizations::IsEmptyForTesting() const {
  return authorized_subresource_urls_.empty();
}

SubresourceUrlAuthorizations::BundleSubresourceInfoAndCount::
    BundleSubresourceInfoAndCount(
        SubresourceUrlBuilder::BundleSubresourceInfo full_info)
    : full_info(std::move(full_info)) {}

}  // namespace content
