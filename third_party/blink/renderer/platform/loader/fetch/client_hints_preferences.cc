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
  DCHECK_EQ(
      static_cast<size_t>(network::mojom::WebClientHintsType::kMaxValue) + 1,
      kClientHintsMappingsCount);
}

void ClientHintsPreferences::UpdateFrom(
    const ClientHintsPreferences& preferences) {
  for (size_t i = 0;
       i < static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) + 1;
       ++i) {
    network::mojom::WebClientHintsType type =
        static_cast<network::mojom::WebClientHintsType>(i);
    enabled_hints_.SetIsEnabled(type, preferences.ShouldSend(type));
  }
}

void ClientHintsPreferences::CombineWith(
    const ClientHintsPreferences& preferences) {
  for (size_t i = 0;
       i < static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) + 1;
       ++i) {
    network::mojom::WebClientHintsType type =
        static_cast<network::mojom::WebClientHintsType>(i);
    if (preferences.ShouldSend(type))
      SetShouldSend(type);
  }
}

void ClientHintsPreferences::UpdateFromHttpEquivAcceptCH(
    const String& header_value,
    const KURL& url,
    Context* context) {
  // Client hints should be allowed only on secure URLs.
  if (!IsClientHintsAllowed(url))
    return;

  // 8-bit conversions from String can turn non-ASCII characters into ?,
  // turning syntax errors into "correct" syntax, so reject those first.
  // (.Utf8() doesn't have this problem, but it does a lot of expensive
  //  work that would be wasted feeding to an ASCII-only syntax).
  if (!header_value.ContainsOnlyASCIIOrEmpty())
    return;

  // Note: .Ascii() would convert tab to ?, which is undesirable.
  absl::optional<std::vector<network::mojom::WebClientHintsType>> parsed_ch =
      network::ParseClientHintsHeader(header_value.Latin1());
  if (!parsed_ch.has_value())
    return;

  // The renderer only handles http-equiv, so this merges.
  for (network::mojom::WebClientHintsType newly_enabled : parsed_ch.value())
    enabled_hints_.SetIsEnabled(newly_enabled, true);

  if (context) {
    for (size_t i = 0;
         i <
         static_cast<int>(network::mojom::WebClientHintsType::kMaxValue) + 1;
         ++i) {
      network::mojom::WebClientHintsType type =
          static_cast<network::mojom::WebClientHintsType>(i);
      if (enabled_hints_.IsEnabled(type))
        context->CountClientHints(type);
    }
  }
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
