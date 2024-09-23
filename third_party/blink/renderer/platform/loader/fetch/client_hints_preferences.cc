// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"

#include "base/command_line.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "url/origin.h"

namespace blink {

ClientHintsPreferences::ClientHintsPreferences() {
  DCHECK_LE(
      network::GetClientHintToNameMap().size(),
      static_cast<size_t>(network::mojom::WebClientHintsType::kMaxValue) + 1);
}

void ClientHintsPreferences::UpdateFrom(
    const ClientHintsPreferences& preferences) {
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& type = elem.first;
    enabled_hints_.SetIsEnabled(type, preferences.ShouldSend(type));
  }
}

void ClientHintsPreferences::CombineWith(
    const ClientHintsPreferences& preferences) {
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& type = elem.first;
    if (preferences.ShouldSend(type))
      SetShouldSend(type);
  }
}

bool ClientHintsPreferences::UpdateFromMetaCH(const String& header_value,
                                              const KURL& url,
                                              Context* context,
                                              network::MetaCHType type,
                                              bool is_doc_preloader,
                                              bool is_sync_parser) {
  // Client hints should be allowed only on secure URLs.
  if (!IsClientHintsAllowed(url))
    return false;

  // 8-bit conversions from String can turn non-ASCII characters into ?,
  // turning syntax errors into "correct" syntax, so reject those first.
  // (.Utf8() doesn't have this problem, but it does a lot of expensive
  //  work that would be wasted feeding to an ASCII-only syntax).
  if (!header_value.ContainsOnlyASCIIOrEmpty())
    return false;

  switch (type) {
    case network::MetaCHType::HttpEquivAcceptCH: {
      // Note: .Ascii() would convert tab to ?, which is undesirable.
      std::optional<std::vector<network::mojom::WebClientHintsType>> parsed_ch =
          network::ParseClientHintsHeader(header_value.Latin1());

      if (!parsed_ch.has_value())
        return false;

      // Update first-party permissions for each client hint.
      for (network::mojom::WebClientHintsType newly_enabled :
           parsed_ch.value()) {
        enabled_hints_.SetIsEnabled(newly_enabled, true);
        if (context && !is_doc_preloader) {
          ukm::builders::ClientHints_AcceptCHMetaUsage(
              context->GetUkmSourceId())
              .SetType(static_cast<int64_t>(newly_enabled))
              .Record(context->GetUkmRecorder());
        }
      }
      break;
    }
    case network::MetaCHType::HttpEquivDelegateCH: {
      if (!is_doc_preloader && !is_sync_parser) {
        break;
      }

      // Note: .Ascii() would convert tab to ?, which is undesirable.
      network::ClientHintToDelegatedThirdPartiesHeader parsed_ch =
          network::ParseClientHintToDelegatedThirdPartiesHeader(
              header_value.Latin1(), type);

      if (parsed_ch.map.empty())
        return false;

      // Update first-party permissions for each client hint.
      for (const auto& pair : parsed_ch.map) {
        enabled_hints_.SetIsEnabled(pair.first, true);
        if (context && !is_doc_preloader) {
          ukm::builders::ClientHints_DelegateCHMetaUsage(
              context->GetUkmSourceId())
              .SetType(static_cast<int64_t>(pair.first))
              .Record(context->GetUkmRecorder());
        }
      }
      break;
    }
  }

  if (context) {
    for (const auto& elem : network::GetClientHintToNameMap()) {
      const auto& hint_type = elem.first;
      if (enabled_hints_.IsEnabled(hint_type))
        context->CountClientHints(hint_type);
    }
  }
  return true;
}

// static
bool ClientHintsPreferences::IsClientHintsAllowed(const KURL& url) {
  // TODO(crbug.com/862940): This should probably be using
  // network::IsUrlPotentiallyTrustworthy() instead of coercing the URL to an
  // origin first.
  return (url.ProtocolIs("http") || url.ProtocolIs("https")) &&
         network::IsOriginPotentiallyTrustworthy(
             url::Origin::Create(GURL(url)));
}

EnabledClientHints ClientHintsPreferences::GetEnabledClientHints() const {
  return enabled_hints_;
}

bool ClientHintsPreferences::ShouldSend(
    network::mojom::WebClientHintsType type) const {
  return enabled_hints_.IsEnabled(type);
}

void ClientHintsPreferences::SetShouldSend(
    network::mojom::WebClientHintsType type) {
  enabled_hints_.SetIsEnabled(type, true);
}

}  // namespace blink
