// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/cross_origin_resource_policy_checker.h"

#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/renderer/core/fetch/response.h"

namespace blink {

CrossOriginResourcePolicyChecker::CrossOriginResourcePolicyChecker(
    network::CrossOriginEmbedderPolicy policy,
    mojo::PendingRemote<
        network::mojom::blink::CrossOriginEmbedderPolicyReporter> reporter)
    : policy_(std::move(policy)) {
  if (reporter) {
    reporter_.Bind(ToCrossVariantMojoType(std::move(reporter)));
  }
}

bool CrossOriginResourcePolicyChecker::IsBlocked(
    const url::Origin& initiator_origin,
    network::mojom::RequestMode request_mode,
    network::mojom::RequestDestination request_destination,
    const blink::Response& response) {
  if (response.InternalURLList().empty()) {
    // The response is synthesized in the service worker, so it's considered as
    // the same origin.
    return false;
  }
  std::optional<std::string> corp_header_value;
  String wtf_corp_header_value;
  if (response.InternalHeaderList()->Get(
          network::CrossOriginResourcePolicy::kHeaderName,
          wtf_corp_header_value)) {
    corp_header_value = wtf_corp_header_value.Utf8();
  }

  return network::CrossOriginResourcePolicy::IsBlockedByHeaderValue(
             GURL(response.InternalURLList().back()),
             GURL(response.InternalURLList().front()), initiator_origin,
             corp_header_value, request_mode, request_destination,
             response.GetResponse()->RequestIncludeCredentials(), policy_,
             reporter_ ? reporter_.get() : nullptr,
             network::DocumentIsolationPolicy())
      .has_value();
}

base::WeakPtr<CrossOriginResourcePolicyChecker>
CrossOriginResourcePolicyChecker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace blink
