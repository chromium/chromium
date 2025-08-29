// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/btm_redirect_info.h"

#include <memory>

#include "base/rand_util.h"
#include "content/browser/btm/btm_utils.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {

BtmRedirectChainInfo::BtmRedirectChainInfo(const GURL& initial_url,
                                           ukm::SourceId initial_source_id,
                                           const GURL& final_url,
                                           ukm::SourceId final_source_id,
                                           size_t length,
                                           bool is_partial_chain,
                                           bool are_3pcs_generally_enabled)
    : chain_id(static_cast<int32_t>(base::RandUint64())),
      initial_url(initial_url),
      initial_source_id(initial_source_id),
      initial_site(GetSiteForBtm(initial_url)),
      final_url(final_url),
      final_source_id(final_source_id),
      final_site(GetSiteForBtm(final_url)),
      initial_and_final_sites_same(initial_site == final_site),
      length(length),
      is_partial_chain(is_partial_chain),
      are_3pcs_generally_enabled(are_3pcs_generally_enabled) {}

BtmRedirectChainInfo::BtmRedirectChainInfo(const BtmRedirectChainInfo&) =
    default;

BtmRedirectChainInfo::~BtmRedirectChainInfo() = default;

/* static */
std::unique_ptr<BtmRedirectInfo> BtmRedirectInfo::CreateForServer(
    const GURL& redirector_url,
    ukm::SourceId redirector_source_id,
    BtmDataAccessType access_type,
    base::Time time,
    bool was_response_cached,
    int response_code,
    base::TimeDelta server_bounce_delay) {
  return base::WrapUnique<BtmRedirectInfo>(new BtmRedirectInfo(
      redirector_url, redirector_source_id,
      /*redirect_type=*/BtmRedirectType::kServer, access_type, time,
      /*client_bounce_delay=*/base::TimeDelta(),
      /*has_sticky_activation=*/false,
      /*web_authn_assertion_request_succeeded=*/false, was_response_cached,
      response_code, server_bounce_delay));
}

/* static */
std::unique_ptr<BtmRedirectInfo> BtmRedirectInfo::CreateForClient(
    const GURL& redirector_url,
    ukm::SourceId redirector_source_id,
    BtmDataAccessType access_type,
    base::Time time,
    base::TimeDelta client_bounce_delay,
    bool has_sticky_activation,
    bool web_authn_assertion_request_succeeded) {
  return base::WrapUnique<BtmRedirectInfo>(new BtmRedirectInfo(
      redirector_url, redirector_source_id,
      /*redirect_type=*/BtmRedirectType::kClient, access_type, time,
      client_bounce_delay, has_sticky_activation,
      web_authn_assertion_request_succeeded,
      /*was_response_cached=*/false,
      /*response_code=*/0,
      /*server_bounce_delay=*/base::TimeDelta()));
}

BtmRedirectInfo::BtmRedirectInfo(const GURL& redirector_url,
                                 ukm::SourceId redirector_source_id,
                                 BtmRedirectType redirect_type,
                                 BtmDataAccessType access_type,
                                 base::Time time,
                                 base::TimeDelta client_bounce_delay,
                                 bool has_sticky_activation,
                                 bool web_authn_assertion_request_succeeded,
                                 bool was_response_cached,
                                 int response_code,
                                 base::TimeDelta server_bounce_delay)
    : redirector_url(redirector_url),
      redirector_source_id(redirector_source_id),
      site(GetSiteForBtm(redirector_url)),
      redirect_type(redirect_type),
      access_type(access_type),
      time(time),
      client_bounce_delay(client_bounce_delay),
      has_sticky_activation(has_sticky_activation),
      web_authn_assertion_request_succeeded(
          web_authn_assertion_request_succeeded),
      was_response_cached(was_response_cached),
      response_code(response_code),
      server_bounce_delay(server_bounce_delay) {}

BtmRedirectInfo::BtmRedirectInfo(const BtmRedirectInfo&) = default;

BtmRedirectInfo::~BtmRedirectInfo() = default;

}  // namespace content
