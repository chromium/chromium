// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request_info.h"

namespace content {

NavigationRequestInfo::NavigationRequestInfo(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    const GURL& site_for_cookies,
    const net::NetworkIsolationKey& network_isolation_key,
    bool is_main_frame,
    bool parent_is_main_frame,
    bool are_ancestors_secure,
    int frame_tree_node_id,
    bool is_for_guests_only,
    bool report_raw_headers,
    bool is_prerendering,
    bool upgrade_if_insecure,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        blob_url_loader_factory,
    const base::UnguessableToken& devtools_navigation_token,
    const base::UnguessableToken& devtools_frame_token,
    bool obey_origin_policy)
    : common_params(std::move(common_params)),
      begin_params(std::move(begin_params)),
      site_for_cookies(site_for_cookies),
      network_isolation_key(network_isolation_key),
      is_main_frame(is_main_frame),
      parent_is_main_frame(parent_is_main_frame),
      are_ancestors_secure(are_ancestors_secure),
      frame_tree_node_id(frame_tree_node_id),
      is_for_guests_only(is_for_guests_only),
      report_raw_headers(report_raw_headers),
      is_prerendering(is_prerendering),
      upgrade_if_insecure(upgrade_if_insecure),
      blob_url_loader_factory(std::move(blob_url_loader_factory)),
      devtools_navigation_token(devtools_navigation_token),
      devtools_frame_token(devtools_frame_token),
      obey_origin_policy(obey_origin_policy) {}

NavigationRequestInfo::~NavigationRequestInfo() {}

}  // namespace content
