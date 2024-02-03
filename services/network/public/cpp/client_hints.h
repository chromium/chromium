// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CLIENT_HINTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CLIENT_HINTS_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"

namespace url {
class Origin;
}  // namespace url

namespace network {

// The "Sec-CH-Prefers-Color-Scheme" header values.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kPrefersColorSchemeDark[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kPrefersColorSchemeLight[];

// The "Sec-CH-Prefers-Reduced-Motion" header values.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kPrefersReducedMotionNoPreference[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kPrefersReducedMotionReduce[];

// The "Sec-CH-Prefers-Reduced-Transparency" header values.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kPrefersReducedTransparencyNoPreference[];
COMPONENT_EXPORT(NETWORK_CPP)
extern const char kPrefersReducedTransparencyReduce[];

// Mapping from WebEffectiveConnectionType to the header value. This value is
// sent to the origins and is returned by the JavaScript API. The ordering
// should match the ordering in //net/nqe/effective_connection_type.h and
// public/platform/WebEffectiveConnectionType.h.
// This array should be updated if either of the enums in
// effective_connection_type.h or WebEffectiveConnectionType.h are updated.
COMPONENT_EXPORT(NETWORK_CPP)
extern const char* const kWebEffectiveConnectionTypeMapping[];

COMPONENT_EXPORT(NETWORK_CPP)
extern const size_t kWebEffectiveConnectionTypeMappingCount;

using ClientHintToNameMap =
    base::flat_map<network::mojom::WebClientHintsType, std::string>;

// Mapping from WebClientHintsType to the hint's name in Accept-CH header.
// The ordering matches the ordering of enums in
// services/network/public/mojom/web_client_hints_types.mojom
COMPONENT_EXPORT(NETWORK_CPP)
const ClientHintToNameMap& GetClientHintToNameMap();

// Tries to parse an Accept-CH header. Returns std::nullopt if parsing
// failed and the header should be ignored; otherwise returns a (possibly
// empty) list of hints to accept.
std::optional<std::vector<network::mojom::WebClientHintsType>> COMPONENT_EXPORT(
    NETWORK_CPP) ParseClientHintsHeader(const std::string& header);

struct COMPONENT_EXPORT(NETWORK_CPP) ClientHintToDelegatedThirdPartiesHeader {
  ClientHintToDelegatedThirdPartiesHeader();
  ~ClientHintToDelegatedThirdPartiesHeader();
  ClientHintToDelegatedThirdPartiesHeader(
      const ClientHintToDelegatedThirdPartiesHeader&);

  base::flat_map<network::mojom::WebClientHintsType, std::vector<url::Origin>>
      map;
  bool had_invalid_origins{false};
};

enum class MetaCHType {
  // This syntax can only activate client hints for the first party origin.
  // <meta http-equiv="accept-ch" content="Sec-CH-DPR">
  // Introduced in M69.
  HttpEquivAcceptCH,
  // This syntax can activate and delegate client hints to any origin.
  // <meta http-equiv="delegate-ch" content="Sec-CH-DPR https://foo.com/">
  // Introduced in M105.
  HttpEquivDelegateCH,
};

// Tries to parse an Accept-CH header w/ third-party delegation ability (i.e. a
// named meta tag). Returns std::nullopt if parsing failed and the header
// should be ignored; otherwise returns a (possibly empty) map of hints to
// delegated third-parties.
const ClientHintToDelegatedThirdPartiesHeader COMPONENT_EXPORT(NETWORK_CPP)
    ParseClientHintToDelegatedThirdPartiesHeader(const std::string& header,
                                                 MetaCHType type);

// This is used by subclassed of ClientHintsControllerDelegate to track the
// amount of time that persisting client hints takes.
void COMPONENT_EXPORT(NETWORK_CPP)
    LogClientHintsPersistenceMetrics(const base::TimeTicks& persistence_started,
                                     std::size_t hints_stored);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CLIENT_HINTS_H_
