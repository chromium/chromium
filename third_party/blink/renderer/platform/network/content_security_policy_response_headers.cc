/*
 * Copyright (C) 2013 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"

#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

namespace blink {

ContentSecurityPolicyResponseHeaders::ContentSecurityPolicyResponseHeaders(
    const ResourceResponse& response)
    : ContentSecurityPolicyResponseHeaders(
          response.HttpHeaderFields(),
          response.CurrentRequestUrl(),
          SchemeRegistry::SchemeSupportsWasmEvalCSP(
              response.CurrentRequestUrl().Protocol())) {}

ContentSecurityPolicyResponseHeaders::ContentSecurityPolicyResponseHeaders(
    const HTTPHeaderMap& headers,
    const KURL& response_url,
    bool should_parse_wasm_eval)
    : content_security_policy_(headers.Get(http_names::kContentSecurityPolicy)),
      content_security_policy_report_only_(
          headers.Get(http_names::kContentSecurityPolicyReportOnly)),
      response_url_(response_url),
      should_parse_wasm_eval_(should_parse_wasm_eval) {}
}
