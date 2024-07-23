// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_attribution_request_helper.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/attribution_reporting/eligibility.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_suitable_context.h"
#include "content/public/browser/global_routing_id.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::RegistrationEligibility;

}  // namespace

// static
std::unique_ptr<KeepAliveAttributionRequestHelper>
KeepAliveAttributionRequestHelper::CreateIfNeeded(
    network::mojom::AttributionReportingEligibility eligibility,
    const GURL& request_url,
    const std::optional<base::UnguessableToken>& attribution_src_token,
    const std::optional<std::string>& devtools_request_id,
    const AttributionSuitableContext& context) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kAttributionReportingInBrowserMigration)) {
    return nullptr;
  }

  std::optional<RegistrationEligibility> registration_eligibility =
      attribution_reporting::GetRegistrationEligibility(eligibility);
  if (!registration_eligibility.has_value()) {
    return nullptr;
  }

  AttributionDataHostManager* data_host_manager = context.data_host_manager();
  if (!data_host_manager) {
    return nullptr;
  }

  std::optional<blink::AttributionSrcToken> token;
  if (attribution_src_token.has_value()) {
    token = blink::AttributionSrcToken(attribution_src_token.value());
  }

  static base::AtomicSequenceNumber unique_id_counter;
  BackgroundRegistrationsId id(unique_id_counter.GetNext());

  data_host_manager->NotifyBackgroundRegistrationStarted(
      id, context, *registration_eligibility, std::move(token),
      devtools_request_id);
  return base::WrapUnique(
      new KeepAliveAttributionRequestHelper(id, data_host_manager,
                                            /*reporting_url=*/request_url));
}

KeepAliveAttributionRequestHelper::KeepAliveAttributionRequestHelper(
    BackgroundRegistrationsId id,
    AttributionDataHostManager* attribution_data_host_manager,
    const GURL& reporting_url)
    : id_(id), reporting_url_(reporting_url) {
  CHECK(attribution_data_host_manager);
  attribution_data_host_manager_ = attribution_data_host_manager->AsWeakPtr();
}

void KeepAliveAttributionRequestHelper::OnReceiveRedirect(
    const net::HttpResponseHeaders* headers,
    const GURL& redirect_url) {
  if (!attribution_data_host_manager_) {
    return;
  }

  attribution_data_host_manager_->NotifyBackgroundRegistrationData(
      id_, headers, reporting_url_);
  reporting_url_ = redirect_url;
}

void KeepAliveAttributionRequestHelper::OnReceiveResponse(
    const net::HttpResponseHeaders* headers) {
  if (!attribution_data_host_manager_) {
    return;
  }

  attribution_data_host_manager_->NotifyBackgroundRegistrationData(
      id_, headers, reporting_url_);
  attribution_data_host_manager_->NotifyBackgroundRegistrationCompleted(id_);
  attribution_data_host_manager_.reset();
}

KeepAliveAttributionRequestHelper::~KeepAliveAttributionRequestHelper() {
  if (!attribution_data_host_manager_) {
    return;
  }
  attribution_data_host_manager_->NotifyBackgroundRegistrationCompleted(id_);
}

}  // namespace content
