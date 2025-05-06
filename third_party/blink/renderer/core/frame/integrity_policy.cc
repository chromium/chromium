// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/integrity_policy.h"

#include "base/containers/contains.h"
#include "services/network/public/cpp/integrity_policy.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/mojom/integrity_algorithm.mojom-blink.h"
#include "services/network/public/mojom/integrity_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// static
bool IntegrityPolicy::AllowRequest(
    ExecutionContext* context,
    network::mojom::RequestDestination request_destination,
    network::mojom::RequestMode request_mode,
    const IntegrityMetadataSet& integrity_metadata,
    const KURL& url) {
  if ((!integrity_metadata.empty() &&
       request_mode != network::mojom::RequestMode::kNoCors) ||
      url.ProtocolIsData() || url.ProtocolIs("blob")) {
    return true;
  }
  PolicyContainer* policy_container = context->GetPolicyContainer();
  const network::IntegrityPolicy& integrity_policy =
      policy_container->GetPolicies().integrity_policy;

  bool allow = true;
  if (request_destination == network::mojom::RequestDestination::kScript) {
    if (base::Contains(
            integrity_policy.blocked_destinations,
            ::network::mojom::blink::IntegrityPolicy::Destination::kScript) &&
        base::Contains(
            integrity_policy.sources,
            ::network::mojom::blink::IntegrityPolicy::Source::kInline)) {
      allow = false;
    }
  }
  return allow;
}

void IntegrityPolicy::LogParsingErrorsIfAny(
    ExecutionContext* context,
    const network::IntegrityPolicy& policy) {
  for (const std::string& error : policy.parsing_errors) {
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError, String(error)));
  }
}

}  // namespace blink
