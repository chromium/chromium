/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"

#include "services/network/public/mojom/cors.mojom-blink.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

OriginAccessEntry::OriginAccessEntry(
    const SecurityOrigin& origin,
    network::mojom::CorsDomainMatchMode match_mode,
    network::mojom::CorsOriginAccessMatchPriority priority)
    : private_(origin.Protocol().Ascii(),
               origin.Domain().Ascii(),
               origin.EffectivePort(),
               match_mode,
               network::mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort,
               priority) {}

OriginAccessEntry::OriginAccessEntry(
    const KURL& url,
    network::mojom::CorsDomainMatchMode match_mode,
    network::mojom::CorsOriginAccessMatchPriority priority)
    : private_(url.Protocol().Ascii().data(),
               url.Host().Ascii().data(),
               url.Port() ? url.Port() : DefaultPortForProtocol(url.Protocol()),
               match_mode,
               network::mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort,
               priority) {}

OriginAccessEntry::OriginAccessEntry(OriginAccessEntry&& from) = default;

network::cors::OriginAccessEntry::MatchResult OriginAccessEntry::MatchesOrigin(
    const SecurityOrigin& origin) const {
  return private_.MatchesOrigin(origin.ToUrlOrigin());
}

network::cors::OriginAccessEntry::MatchResult OriginAccessEntry::MatchesDomain(
    const SecurityOrigin& origin) const {
  return private_.MatchesDomain(origin.Host().Ascii());
}

bool OriginAccessEntry::HostIsIPAddress() const {
  return private_.host_is_ip_address();
}

String OriginAccessEntry::registrable_domain() const {
  return String(private_.registrable_domain().c_str());
}

}  // namespace blink
