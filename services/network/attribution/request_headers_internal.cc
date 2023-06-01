// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/request_headers_internal.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/types/cxx23_to_underlying.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

namespace {

using GreaseContext =
    ::network::AttributionReportingEligibleGreaseOptions::GreaseContext;

using ::network::mojom::AttributionReportingEligibility;

void AddTrueValuedDictMember(
    std::vector<net::structured_headers::DictionaryMember>& dict,
    std::string key) {
  dict.emplace_back(std::move(key),
                    net::structured_headers::ParameterizedMember(
                        net::structured_headers::Item(true),
                        net::structured_headers::Parameters()));
}

}  // namespace

// static
AttributionReportingEligibleGreaseOptions
AttributionReportingEligibleGreaseOptions::FromBits(uint64_t bits) {
  AttributionReportingEligibleGreaseOptions options;

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
    const AttributionReportingEligibleGreaseOptions& options) {
  const char* const kEventSource = "event-source";
  const char* const kNavigationSource = "navigation-source";
  const char* const kTrigger = "trigger";

  std::vector<net::structured_headers::DictionaryMember> eligibilities;

  const char* grease1;
  const char* grease2;
  switch (eligibility) {
    case AttributionReportingEligibility::kUnset:
      NOTREACHED_NORETURN();
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

  // We "grease" the header with meaningless components to help ensure that
  // recipients are using a structured header parser, not naive string
  // operations on the serialized header, and to ensure that values and
  // parameters are ignored. We carefully choose the greases themselves to
  // minimize excessive bandwidth: each substring "event-source",
  // "navigation-source", and "trigger" will appear at most once in the
  // serialized header.
  //
  // https://wicg.github.io/attribution-reporting-api/#set-attribution-reporting-headers

  // Allowing these to be swapped gives them equal chance of being used, since
  // otherwise a key is more likely to exist for the second grease than the
  // first.
  if (options.swap_greases) {
    std::swap(grease1, grease2);
  }

  const auto apply_grease = [&](GreaseContext context, bool use_front,
                                const char* grease) {
    if (!grease) {
      return;
    }

    switch (context) {
      case GreaseContext::kNone:
        break;
      case GreaseContext::kKey:
        AddTrueValuedDictMember(eligibilities, base::StrCat({"not-", grease}));
        break;
      case GreaseContext::kValue:
        if (!eligibilities.empty()) {
          auto& [_, parameterized_member] =
              use_front ? eligibilities.front() : eligibilities.back();
          parameterized_member.member.front().item =
              net::structured_headers::Item(
                  grease, net::structured_headers::Item::kTokenType);
        }
        break;
      case GreaseContext::kParamName:
        if (!eligibilities.empty()) {
          auto& [_, parameterized_member] =
              use_front ? eligibilities.front() : eligibilities.back();
          parameterized_member.params.emplace_back(
              grease, net::structured_headers::Item(true));
        }
        break;
    }
  };

  apply_grease(options.context1, options.use_front1, grease1);
  apply_grease(options.context2, options.use_front2, grease2);

  if (eligibilities.size() > 1 && options.reverse) {
    // Dictionaries retain order during serialization, so reordering helps
    // ensure that recipients do not depend on it.
    base::ranges::reverse(eligibilities);
  }

  absl::optional<std::string> eligible_header =
      net::structured_headers::SerializeDictionary(
          net::structured_headers::Dictionary(std::move(eligibilities)));
  DCHECK(eligible_header.has_value());

  return std::move(*eligible_header);
}

std::string GetAttributionSupportHeader(
    mojom::AttributionSupport attribution_support) {
  std::vector<net::structured_headers::DictionaryMember> registrars;

  if (HasAttributionOsSupport(attribution_support)) {
    AddTrueValuedDictMember(registrars, "os");
  }
  if (HasAttributionWebSupport(attribution_support)) {
    AddTrueValuedDictMember(registrars, "web");
  }

  absl::optional<std::string> support_header =
      net::structured_headers::SerializeDictionary(
          net::structured_headers::Dictionary(std::move(registrars)));
  DCHECK(support_header.has_value());
  return std::move(*support_header);
}

}  // namespace network
