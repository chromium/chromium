// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_registration_handler.h"

#include "content/browser/webid/config_fetcher.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "url/gurl.h"

namespace content::webid {

IdpRegistrationHandler::IdpRegistrationHandler(
    RenderFrameHost& render_frame_host,
    IdpNetworkRequestManager* network_manager,
    const GURL& idp_url)
    : render_frame_host_(render_frame_host),
      network_manager_(network_manager),
      idp_url_(idp_url) {}

IdpRegistrationHandler::~IdpRegistrationHandler() = default;

void IdpRegistrationHandler::FetchConfig(
    ConfigFetcher::RequesterCallback callback) {
  std::vector<ConfigFetcher::FetchRequest> fetch_requests;
  fetch_requests.emplace_back(idp_url_,
                              /*force_skip_well_known_enforcement=*/true);

  config_fetcher_ =
      std::make_unique<ConfigFetcher>(*render_frame_host_, network_manager_);
  config_fetcher_->Start(fetch_requests, blink::mojom::RpMode::kPassive,
                         /*icon_ideal_size=*/0, /*icon_minimum_size=*/0,
                         std::move(callback));
}

}  // namespace content::webid
