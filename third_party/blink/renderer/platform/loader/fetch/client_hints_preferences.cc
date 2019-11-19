// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"

#include "base/macros.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

void ParseAcceptChHeader(const String& header_value,
                         WebEnabledClientHints& enabled_hints) {
  CommaDelimitedHeaderSet accept_client_hints_header;
  ParseCommaDelimitedHeader(header_value, accept_client_hints_header);

  for (size_t i = 0;
       i < static_cast<int>(mojom::WebClientHintsType::kMaxValue) + 1; ++i) {
    enabled_hints.SetIsEnabled(
        static_cast<mojom::WebClientHintsType>(i),
        accept_client_hints_header.Contains(kClientHintsNameMapping[i]));
  }

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kDeviceMemory,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kDeviceMemory));

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kRtt,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kRtt));

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kDownlink,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kDownlink));

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kEct,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kEct));

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kLang,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kLang) &&
          RuntimeEnabledFeatures::LangClientHintHeaderEnabled());

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kUA,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kUA) &&
          RuntimeEnabledFeatures::UserAgentClientHintEnabled());

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kUAArch,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kUAArch) &&
          RuntimeEnabledFeatures::UserAgentClientHintEnabled());

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kUAPlatform,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kUAPlatform) &&
          RuntimeEnabledFeatures::UserAgentClientHintEnabled());

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kUAModel,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kUAModel) &&
          RuntimeEnabledFeatures::UserAgentClientHintEnabled());
}

}  // namespace

ClientHintsPreferences::ClientHintsPreferences() {
  DCHECK_EQ(static_cast<size_t>(mojom::WebClientHintsType::kMaxValue) + 1,
            kClientHintsMappingsCount);
}

void ClientHintsPreferences::UpdateFrom(
    const ClientHintsPreferences& preferences) {
  for (size_t i = 0;
       i < static_cast<int>(mojom::WebClientHintsType::kMaxValue) + 1; ++i) {
    mojom::WebClientHintsType type = static_cast<mojom::WebClientHintsType>(i);
    enabled_hints_.SetIsEnabled(type, preferences.ShouldSend(type));
  }
}

void ClientHintsPreferences::UpdateFromAcceptClientHintsHeader(
    const String& header_value,
    const KURL& url,
    Context* context) {
  if (header_value.IsEmpty())
    return;

  // Client hints should be allowed only on secure URLs.
  if (!IsClientHintsAllowed(url))
    return;

  WebEnabledClientHints new_enabled_types;

  ParseAcceptChHeader(header_value, new_enabled_types);

  for (size_t i = 0;
       i < static_cast<int>(mojom::WebClientHintsType::kMaxValue) + 1; ++i) {
    mojom::WebClientHintsType type = static_cast<mojom::WebClientHintsType>(i);
    enabled_hints_.SetIsEnabled(type, enabled_hints_.IsEnabled(type) ||
                                          new_enabled_types.IsEnabled(type));
  }

  if (context) {
    for (size_t i = 0;
         i < static_cast<int>(mojom::WebClientHintsType::kMaxValue) + 1; ++i) {
      mojom::WebClientHintsType type =
          static_cast<mojom::WebClientHintsType>(i);
      if (enabled_hints_.IsEnabled(type))
        context->CountClientHints(type);
    }
  }
}

void ClientHintsPreferences::UpdateFromAcceptClientHintsLifetimeHeader(
    const String& header_value,
    const KURL& url,
    Context* context) {
  if (header_value.IsEmpty())
    return;

  // Client hints should be allowed only on secure URLs.
  if (!IsClientHintsAllowed(url))
    return;

  bool conversion_ok = false;
  int64_t persist_duration_seconds = header_value.ToInt64Strict(&conversion_ok);
  if (!conversion_ok || persist_duration_seconds <= 0)
    return;

  persist_duration_ = base::TimeDelta::FromSeconds(persist_duration_seconds);
  if (context)
    context->CountPersistentClientHintHeaders();
}

// static
bool ClientHintsPreferences::IsClientHintsAllowed(const KURL& url) {
  return (url.ProtocolIs("http") || url.ProtocolIs("https")) &&
         (SecurityOrigin::IsSecure(url) ||
          SecurityOrigin::Create(url)->IsLocalhost());
}

WebEnabledClientHints ClientHintsPreferences::GetWebEnabledClientHints() const {
  return enabled_hints_;
}

base::TimeDelta ClientHintsPreferences::GetPersistDuration() const {
  return persist_duration_;
}

}  // namespace blink
