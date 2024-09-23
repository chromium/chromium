// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/request_headers_internal.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/types/cxx23_to_underlying.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/attribution.mojom.h"

namespace network {

namespace {

using GreaseContext =
    ::network::AttributionReportingHeaderGreaseOptions::GreaseContext;

using ::network::mojom::AttributionReportingEligibility;

void AddTrueValuedDictMember(
    std::vector<net::structured_headers::DictionaryMember>& dict,
    std::string key) {
  dict.emplace_back(std::move(key),
                    net::structured_headers::ParameterizedMember(
                        net::structured_headers::Item(true),
                        net::structured_headers::Parameters()));
}

void ApplyGreaseContext(
    std::vector<net::structured_headers::DictionaryMember>& dict,
    GreaseContext context,
    bool use_front,
    const char* grease) {
  if (!grease) {
    return;
  }

  switch (context) {
    case GreaseContext::kNone:
      break;
    case GreaseContext::kKey:
      AddTrueValuedDictMember(dict, base::StrCat({"not-", grease}));
      break;
    case GreaseContext::kValue:
      if (!dict.empty()) {
        auto& [_, parameterized_member] =
            use_front ? dict.front() : dict.back();
        parameterized_member.member.front().item =
            net::structured_headers::Item(
                grease, net::structured_headers::Item::kTokenType);
      }
      break;
    case GreaseContext::kParamName:
      if (!dict.empty()) {
        auto& [_, parameterized_member] =
            use_front ? dict.front() : dict.back();
        parameterized_member.params.emplace_back(
            grease, net::structured_headers::Item(true));
      }
      break;
  }
}

void ApplyGrease(std::vector<net::structured_headers::DictionaryMember>& dict,
                 const AttributionReportingHeaderGreaseOptions& options,
                 const char* grease1,
                 const char* grease2) {
  // We "grease" the header with meaningless components to help ensure that
  // recipients are using a structured header parser, not naive string
  // operations on the serialized header, and to ensure that values and
  // parameters are ignored. We carefully choose the greases themselves to
  // minimize excessive bandwidth: each substring corresponding to a real key
  // will appear at most once in the serialized header.
  //
  // https://wicg.github.io/attribution-reporting-api/#set-attribution-reporting-headers

  // Allowing these to be swapped gives them equal chance of being used, since
  // otherwise a key is more likely to exist for the second grease than the
  // first.
  if (options.swap_greases) {
    std::swap(grease1, grease2);
  }

  ApplyGreaseContext(dict, options.context1, options.use_front1, grease1);
  ApplyGreaseContext(dict, options.context2, options.use_front2, grease2);

  if (dict.size() > 1 && options.reverse) {
    // Dictionaries retain order during serialization, so reordering helps
    // ensure that recipients do not depend on it.
    base::ranges::reverse(dict);
  }
}

}  // namespace

// static
AttributionReportingHeaderGreaseOptions
AttributionReportingHeaderGreaseOptions::FromBits(uint8_t bits) {
  AttributionReportingHeaderGreaseOptions options;

  options.reverse = bits & 0b1;
  bits >>= 1;

  options.swap_greases = bits & 0b1;
  bits >>= 1;

  static_assert(base::to_underlying(GreaseContext::kNone) == 0);
  static_assert(base::to_underlying(GreaseContext::kKey) == 1);
  static_assert(base::to_underlying(GreaseContext::kValue) == 2);
  static_assert(base::to_underlying(GreaseContext::kParamName) == 3);

  options.context1 = static_cast<GreaseContext>(bits & 0b11);
  bits >>= 2;

  options.context2 = static_cast<GreaseContext>(bits & 0b11);
  bits >>= 2;

  options.use_front1 = bits & 0b1;
  bits >>= 1;

  options.use_front2 = bits & 0b1;
  bits >>= 1;

  return options;
}

std::string SerializeAttributionReportingEligibleHeader(
    const AttributionReportingEligibility eligibility,
    const AttributionReportingHeaderGreaseOptions& options) {
  const char* const kEventSource = "event-source";
  const char* const kNavigationSource = "navigation-source";
  const char* const kTrigger = "trigger";

  std::vector<net::structured_headers::DictionaryMember> eligibilities;

  const char* grease1;
  const char* grease2;
  switch (eligibility) {
    case AttributionReportingEligibility::kUnset:
      NOTREACHED();
    case AttributionReportingEligibility::kEmpty:
      grease1 = kEventSource;
      grease2 = kTrigger;
      break;
    case AttributionReportingEligibility::kEventSource:
      AddTrueValuedDictMember(eligibilities, kEventSource);
      grease1 = kTrigger;
      grease2 = kNavigationSource;
      break;
    case AttributionReportingEligibility::kNavigationSource:
      AddTrueValuedDictMember(eligibilities, kNavigationSource);
      grease1 = kEventSource;
      grease2 = kTrigger;
      break;
    case AttributionReportingEligibility::kTrigger:
      AddTrueValuedDictMember(eligibilities, kTrigger);
      grease1 = kNavigationSource;
      grease2 = kEventSource;
      break;
    case AttributionReportingEligibility::kEventSourceOrTrigger:
      AddTrueValuedDictMember(eligibilities, kEventSource);
      AddTrueValuedDictMember(eligibilities, kTrigger);
      grease1 = kNavigationSource;
      grease2 = nullptr;
      break;
  }

  ApplyGrease(eligibilities, options, grease1, grease2);

  std::optional<std::string> eligible_header =
      net::structured_headers::SerializeDictionary(
          net::structured_headers::Dictionary(std::move(eligibilities)));
  DCHECK(eligible_header.has_value());

  return std::move(*eligible_header);
}

std::string GetAttributionSupportHeader(
    mojom::AttributionSupport attribution_support,
    const AttributionReportingHeaderGreaseOptions& options) {
  std::vector<net::structured_headers::DictionaryMember> registrars;

  const char* grease1;
  const char* grease2;
  switch (attribution_support) {
    case mojom::AttributionSupport::kWeb:
      AddTrueValuedDictMember(registrars, "web");
      grease1 = "os";
      grease2 = nullptr;
      break;
    case mojom::AttributionSupport::kOs:
      AddTrueValuedDictMember(registrars, "os");
      grease1 = "web";
      grease2 = nullptr;
      break;
    case mojom::AttributionSupport::kWebAndOs:
      AddTrueValuedDictMember(registrars, "os");
      AddTrueValuedDictMember(registrars, "web");
      grease1 = nullptr;
      grease2 = nullptr;
      break;
    case mojom::AttributionSupport::kNone:
      grease1 = "os";
      grease2 = "web";
      break;
    case mojom::AttributionSupport::kUnset:
      NOTREACHED();
  }

  ApplyGrease(registrars, options, grease1, grease2);

  std::optional<std::string> support_header =
      net::structured_headers::SerializeDictionary(
          net::structured_headers::Dictionary(std::move(registrars)));
  DCHECK(support_header.has_value());
  return std::move(*support_header);
}

}  // namespace network
