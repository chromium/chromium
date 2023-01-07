// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"

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
}

ContentSecurityPolicyResponseHeaders
InstalledScriptsManager::ScriptData::GetContentSecurityPolicyResponseHeaders() {
  return ContentSecurityPolicyResponseHeaders(headers_, script_url_);
}

String InstalledScriptsManager::ScriptData::GetReferrerPolicy() {
  return headers_.Get(http_names::kReferrerPolicy);
}

String InstalledScriptsManager::ScriptData::GetHttpContentType() {
  // Strip charset parameters from the MIME type since MIMETypeRegistry does
  // not expect them to be present.
  return ExtractMIMETypeFromMediaType(headers_.Get(http_names::kContentType));
}

std::unique_ptr<Vector<String>>
InstalledScriptsManager::ScriptData::CreateOriginTrialTokens() {
  return OriginTrialContext::ParseHeaderValue(
      headers_.Get(http_names::kOriginTrial));
}

}  // namespace blink
