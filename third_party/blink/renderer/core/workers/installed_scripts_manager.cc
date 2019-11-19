// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"

#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"

namespace blink {

InstalledScriptsManager::ScriptData::ScriptData(
    const KURL& script_url,
    String source_text,
    std::unique_ptr<Vector<uint8_t>> meta_data,
    std::unique_ptr<CrossThreadHTTPHeaderMapData> header_data)
    : script_url_(script_url),
      source_text_(std::move(source_text)),
      meta_data_(std::move(meta_data)) {
  headers_.Adopt(std::move(header_data));

  // Calculate an address space from worker script's response url according to
  // the "CORS and RFC1918" spec:
  // https://wicg.github.io/cors-rfc1918/#integration-html
  //
  // Currently this implementation is not fully consistent with the spec for
  // historical reasons.
  // TODO(https://crbug.com/955213): Make this consistent with the spec.
  // TODO(https://crbug.com/955213): Move this function to a more appropriate
  // place so that this is shareable out of worker code.
  response_address_space_ = network::mojom::IPAddressSpace::kPublic;
  if (network_utils::IsReservedIPAddress(script_url_.Host()))
    response_address_space_ = network::mojom::IPAddressSpace::kPrivate;
  if (SecurityOrigin::Create(script_url_)->IsLocalhost())
    response_address_space_ = network::mojom::IPAddressSpace::kLocal;
}

ContentSecurityPolicyResponseHeaders
InstalledScriptsManager::ScriptData::GetContentSecurityPolicyResponseHeaders() {
  return ContentSecurityPolicyResponseHeaders(headers_);
}

String InstalledScriptsManager::ScriptData::GetReferrerPolicy() {
  return headers_.Get(http_names::kReferrerPolicy);
}

std::unique_ptr<Vector<String>>
InstalledScriptsManager::ScriptData::CreateOriginTrialTokens() {
  return OriginTrialContext::ParseHeaderValue(
      headers_.Get(http_names::kOriginTrial));
}

}  // namespace blink
