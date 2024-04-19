// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_SUBRESOURCE_URL_BUILDER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_SUBRESOURCE_URL_BUILDER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Produces and stores the BundleSubresourceInfo for each signal in
// DirectFromSellerSignals.
class CONTENT_EXPORT SubresourceUrlBuilder {
 public:
  // Holds all the information needed to request a subresource from the browser
  // process, except the renderer process ID (which is held by the
  // AuctionURLLoaderFactoryProxy, since it is per-frame).
  //
  // TODO(crbug.com/40860075): Once subresource bundles support shared ownership
  // handle types, replace this class with that handle type. That way, this
  // struct will also hold shared ownership of all the subresource bundles.
  struct CONTENT_EXPORT BundleSubresourceInfo {
    BundleSubresourceInfo(
        const GURL& subresource_url,
        const blink::DirectFromSellerSignalsSubresource& info_from_renderer);
    ~BundleSubresourceInfo();

    BundleSubresourceInfo(const BundleSubresourceInfo&);
    BundleSubresourceInfo& operator=(const BundleSubresourceInfo&);
    BundleSubresourceInfo(BundleSubresourceInfo&&);
    BundleSubresourceInfo& operator=(BundleSubresourceInfo&&);

    // The subresource URL, as constructed by the browser using the prefix +
    // suffix, as described in the DirectFromSellerSignals mojom comments.
    GURL subresource_url;

    // The bundle_url and token from the renderer process.
    blink::DirectFromSellerSignalsSubresource info_from_renderer;
  };

  explicit SubresourceUrlBuilder(
      const std::optional<blink::DirectFromSellerSignals>&
          direct_from_seller_signals);
  ~SubresourceUrlBuilder();

  SubresourceUrlBuilder(const SubresourceUrlBuilder&) = delete;
  SubresourceUrlBuilder& operator=(const SubresourceUrlBuilder&) = delete;

  const std::optional<BundleSubresourceInfo>& seller_signals() const {
    return seller_signals_;
  }

  const std::optional<BundleSubresourceInfo>& auction_signals() const {
    return auction_signals_;
  }

  const base::flat_map<url::Origin, BundleSubresourceInfo>& per_buyer_signals()
      const {
    return per_buyer_signals_;
  }

 private:
  const std::optional<BundleSubresourceInfo> seller_signals_;
  const std::optional<BundleSubresourceInfo> auction_signals_;
  const base::flat_map<url::Origin, BundleSubresourceInfo> per_buyer_signals_;
};

bool CONTENT_EXPORT
operator==(const SubresourceUrlBuilder::BundleSubresourceInfo& a,
           const SubresourceUrlBuilder::BundleSubresourceInfo& b);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_SUBRESOURCE_URL_BUILDER_H_
