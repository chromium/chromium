// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"

#include "base/command_line.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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

bool ClientHintsPreferences::UpdateFromMetaTagAcceptCH(
    const String& header_value,
    const KURL& url,
    Context* context,
    bool is_http_equiv,
    bool is_preload_or_sync_parser) {
  // Client hints should be allowed only on secure URLs.
  if (!IsClientHintsAllowed(url))
    return false;

  // 8-bit conversions from String can turn non-ASCII characters into ?,
  // turning syntax errors into "correct" syntax, so reject those first.
  // (.Utf8() doesn't have this problem, but it does a lot of expensive
  //  work that would be wasted feeding to an ASCII-only syntax).
  if (!header_value.ContainsOnlyASCIIOrEmpty())
    return false;

  if (is_http_equiv) {
    // Note: .Ascii() would convert tab to ?, which is undesirable.
    absl::optional<std::vector<network::mojom::WebClientHintsType>> parsed_ch =
        network::ParseClientHintsHeader(header_value.Latin1());

    if (!parsed_ch.has_value())
      return false;

    // Update first-party permissions for each client hint.
    for (network::mojom::WebClientHintsType newly_enabled : parsed_ch.value()) {
      enabled_hints_.SetIsEnabled(newly_enabled, true);
    }
  } else if (is_preload_or_sync_parser) {
    // Note: .Ascii() would convert tab to ?, which is undesirable.
    absl::optional<network::ClientHintToDelegatedThirdPartiesHeader> parsed_ch =
        network::ParseClientHintToDelegatedThirdPartiesHeader(
            header_value.Latin1());

    if (!parsed_ch.has_value())
      return false;

    // Update first-party permissions for each client hint.
    for (const auto& pair : parsed_ch.value().map) {
      enabled_hints_.SetIsEnabled(pair.first, true);
    }
  }

  if (context) {
    for (const auto& elem : network::GetClientHintToNameMap()) {
      const auto& type = elem.first;
      if (enabled_hints_.IsEnabled(type))
        context->CountClientHints(type);
    }
  }
  return true;
}

// static
bool ClientHintsPreferences::IsClientHintsAllowed(const KURL& url) {
  return (url.ProtocolIs("http") || url.ProtocolIs("https")) &&
         network::IsOriginPotentiallyTrustworthy(url::Origin::Create(url));
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
