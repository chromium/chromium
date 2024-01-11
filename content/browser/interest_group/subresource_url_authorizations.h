// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_SUBRESOURCE_URL_AUTHORIZATIONS_H_
#define CONTENT_BROWSER_INTEREST_GROUP_SUBRESOURCE_URL_AUTHORIZATIONS_H_

#include <map>
#include <vector>

#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/common/content_export.h"

namespace content {

// Manages the subresource URLs that may be fetched by the worklet process (for
// a given WorkletOwner).
//
// Owned by the AuctionURLLoaderFactoryProxy.
class CONTENT_EXPORT SubresourceUrlAuthorizations {
 public:
  SubresourceUrlAuthorizations();
  ~SubresourceUrlAuthorizations();

  explicit SubresourceUrlAuthorizations(const SubresourceUrlAuthorizations&) =
      delete;
  SubresourceUrlAuthorizations& operator=(const SubresourceUrlAuthorizations&) =
      delete;

  // Returns the BundleSubresourceInfo for `subresource_url` iff the worklet is
  // authorized to access `subresource_url`, otherwise returns std::nullopt. To
  // be called by the AuctionURLLoaderFactoryProxy.
  //
  // Returned pointer is invaliadted if AuthorizeSubresourceUrls() or
  // OnWorkletHandleDestruction() is called.
  const SubresourceUrlBuilder::BundleSubresourceInfo* GetAuthorizationInfo(
      const GURL& subresource_url) const;

  // Returns true if no URLs are authorized. NOTE: this can return true even if
  // the WorkletHandle map is not empty -- but every WorkletHandle must have
  // authorized 0 URLs.
  bool IsEmptyForTesting() const;

 private:
  friend class AuctionWorkletManager::WorkletHandle;
  friend class SubresourceUrlAuthorizationsTest;
  friend class AuctionUrlLoaderFactoryProxyTest;

  struct BundleSubresourceInfoAndCount {
    explicit BundleSubresourceInfoAndCount(
        SubresourceUrlBuilder::BundleSubresourceInfo full_info);

    SubresourceUrlBuilder::BundleSubresourceInfo full_info;
    int count = 0;
  };

  // Below are called by WorkletHandle and tests via friendship.

  // Authorize the worklet to access all subresource URLs in
  // `authorized_subresource_urls` for the duration of the lifetime of
  // `worklet_handle`.
  //
  // If a registration already exists for `worklet_handle` does nothing.
  void AuthorizeSubresourceUrls(
      const AuctionWorkletManager::WorkletHandle* worklet_handle,
      const std::vector<SubresourceUrlBuilder::BundleSubresourceInfo>&
          authorized_subresource_urls);

  // Unregisters `worklet_handle` and decrements the counts of all subresource
  // URLs registered by `worklet_handle` -- any subresource URLs whose counts
  // that reach 0 will be removed.
  //
  // To be called by the WorkletHandle destructor.
  void OnWorkletHandleDestruction(
      const AuctionWorkletManager::WorkletHandle* worklet_handle);

  // Tracks the subresource URLs associated with the given WorkletHandle so that
  // the `authorized_subresource_urls_` counts for those subresource
  // URLs can be decremented when the WorkletHandle is
  // destroyed.
  std::map<const AuctionWorkletManager::WorkletHandle*, std::vector<GURL>>
      subresource_urls_per_handle_;

  // Stores as keys the list of all subresource URLs that may be accessed by the
  // worklet associated with the AuctionURLLoaderFactoryProxy that owns this
  // SubresourceUrlAuthorizations.
  //
  // The mapped value keeps the BundleSubresourceInfo needed to access the
  // subresource URL and a count of how many WorkletHandles have authorized the
  // given subresource URL key -- when the count decrements to 0, the pair is
  // removed.
  std::map<GURL, BundleSubresourceInfoAndCount> authorized_subresource_urls_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_SUBRESOURCE_URL_AUTHORIZATIONS_H_
