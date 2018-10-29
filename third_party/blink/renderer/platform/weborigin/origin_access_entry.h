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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_ORIGIN_ACCESS_ENTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_ORIGIN_ACCESS_ENTRY_H_

#include "services/network/public/cpp/cors/origin_access_entry.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SecurityOrigin;

// A class to wrap network::cors::OriginAccessEntry to use with Blink types.
class PLATFORM_EXPORT OriginAccessEntry {
  USING_FAST_MALLOC(OriginAccessEntry);

 public:
  // If host is empty string and MatchMode is not DisallowSubdomains, the entry
  // will match all domains in the specified protocol.
  // IPv6 addresses must include brackets (e.g.
  // '[2001:db8:85a3::8a2e:370:7334]', not '2001:db8:85a3::8a2e:370:7334').
  // An entry with a higher priority will win in case there are two conflicting
  // entries.
  OriginAccessEntry(
      const String& protocol,
      const String& host,
      network::cors::OriginAccessEntry::MatchMode,
      network::mojom::CORSOriginAccessMatchPriority priority =
          network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  OriginAccessEntry(OriginAccessEntry&& from);

  // 'matchesOrigin' requires a protocol match (e.g. 'http' != 'https').
  // 'matchesDomain' relaxes this constraint.
  network::cors::OriginAccessEntry::MatchResult MatchesOrigin(
      const SecurityOrigin&) const;
  network::cors::OriginAccessEntry::MatchResult MatchesDomain(
      const SecurityOrigin&) const;

  bool HostIsIPAddress() const;

 private:
  network::cors::OriginAccessEntry private_;

  DISALLOW_COPY_AND_ASSIGN(OriginAccessEntry);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_ORIGIN_ACCESS_ENTRY_H_
