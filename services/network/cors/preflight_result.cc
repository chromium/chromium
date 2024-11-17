// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_result.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/cors/cors_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"

namespace network::cors {

namespace {

// Timeout values below are at the discretion of the user agent.

// Default cache expiry time for an entry that does not have
// Access-Control-Max-Age header in its CORS-preflight response.
constexpr base::TimeDelta kDefaultTimeout = base::Seconds(5);

// Maximum cache expiry time. Even if a CORS-preflight response contains
// Access-Control-Max-Age header that specifies a longer expiry time, this
// maximum time is applied.
constexpr base::TimeDelta kMaxTimeout = base::Hours(2);

// Holds TickClock instance to overwrite TimeTicks::Now() for testing.
const base::TickClock* tick_clock_for_testing = nullptr;

// We define the value here because we want a lower-cased header name.
constexpr char kAuthorization[] = "authorization";

base::TimeTicks Now() {
  if (tick_clock_for_testing)
    return tick_clock_for_testing->NowTicks();
  return base::TimeTicks::Now();
}

base::TimeDelta ParseAccessControlMaxAge(
    const std::optional<std::string>& max_age) {
  if (!max_age) {
    return kDefaultTimeout;
  }

  int64_t seconds;
  if (!base::StringToInt64(*max_age, &seconds)) {
    return kDefaultTimeout;
  }

  // Negative value doesn't make sense - use 0 instead, to represent that the
  // entry cannot be cached.
  if (seconds < 0) {
    return base::TimeDelta();
  }
  // To avoid integer overflow, we compare seconds instead of comparing
  // TimeDeltas.
  static_assert(kMaxTimeout == base::Seconds(kMaxTimeout.InSeconds()),
                "`kMaxTimeout` must be a multiple of one second.");
  if (seconds >= kMaxTimeout.InSeconds()) {
    return kMaxTimeout;
  }

  return base::Seconds(seconds);
}

// Parses `string` as a Access-Control-Allow-* header value, storing the result
// in `set`. This function returns false when `string` does not satisfy the
// syntax here: https://fetch.spec.whatwg.org/#http-new-header-syntax.
bool ParseAccessControlAllowList(const std::optional<std::string>& string,
                                 base::flat_set<std::string>* set,
                                 bool insert_in_lower_case) {
  DCHECK(set);

  if (!string)
    return true;

  net::HttpUtil::ValuesIterator it(*string, /*delimiter=*/',', true);
  while (it.GetNext()) {
    std::string_view value = it.value();
    if (!net::HttpUtil::IsToken(value)) {
      set->clear();
      return false;
    }
    set->insert(insert_in_lower_case ? base::ToLowerASCII(value)
                                     : std::string(value));
  }
  return true;
}

// Joins the strings in the given `set ` with commas.
std::string JoinSet(const base::flat_set<std::string>& set) {
  std::vector<std::string_view> values(set.begin(), set.end());
  return base::JoinString(values, ",");
}

}  // namespace

// static
void PreflightResult::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_for_testing = tick_clock;
}

// static
std::unique_ptr<PreflightResult> PreflightResult::Create(
    const mojom::CredentialsMode credentials_mode,
    const std::optional<std::string>& allow_methods_header,
    const std::optional<std::string>& allow_headers_header,
    const std::optional<std::string>& max_age_header,
    std::optional<mojom::CorsError>* detected_error) {
  std::unique_ptr<PreflightResult> result =
      base::WrapUnique(new PreflightResult(credentials_mode));
  std::optional<mojom::CorsError> error =
      result->Parse(allow_methods_header, allow_headers_header, max_age_header);
  if (error) {
    if (detected_error)
      *detected_error = error;
    return nullptr;
  }
  return result;
}

PreflightResult::PreflightResult(const mojom::CredentialsMode credentials_mode)
    : credentials_(credentials_mode == mojom::CredentialsMode::kInclude) {}

PreflightResult::~PreflightResult() = default;

std::optional<CorsErrorStatus> PreflightResult::EnsureAllowedCrossOriginMethod(
    const std::string& method,
    bool acam_preflight_spec_conformant) const {
  // `normalized_method_allowed`: Request method is normalized to upper case,
  // and comparison is performed in case-sensitive way, that means access
  // control header should provide an upper case method list. This behavior is
  // to be deprecated (https://crbug.com/1228178).
  const std::string normalized_method = base::ToUpperASCII(method);
  const bool normalized_method_allowed =
      methods_.find(normalized_method) != methods_.end() ||
      IsCorsSafelistedMethod(normalized_method);

  // `method_allowed`: Request method should be already normalized (in Blink, in
  // https://xhr.spec.whatwg.org/#dom-xmlhttprequest-open or
  // https://fetch.spec.whatwg.org/#dom-request) so we don't normalize the
  // method again here.
  const bool method_allowed =
      methods_.find(method) != methods_.end() || IsCorsSafelistedMethod(method);

  // This should be consistent with `NetworkServiceCorsPreflightMethodAllowed`
  // in `tools/metrics/histograms/enums.xml`.
  enum CorsPreflightMethodAllowed : uint8_t {
    kBothDisallowed = 0,
    kNormalizedMethodAllowed = 1,
    kMethodAllowed = 2,
    kBothAllowed = 3,
    kMaxValue = kBothAllowed
  };

  UMA_HISTOGRAM_ENUMERATION(
      "NetworkService.CorsPreflightMethodAllowed",
      normalized_method_allowed
          ? (method_allowed ? kBothAllowed : kNormalizedMethodAllowed)
          : (method_allowed ? kMethodAllowed : kBothDisallowed));

  const bool allowed = acam_preflight_spec_conformant
                           ? method_allowed
                           : normalized_method_allowed;

  if (allowed) {
    return std::nullopt;
  }

  if (!credentials_ && methods_.find("*") != methods_.end())
    return std::nullopt;

  return CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                         method);
}

std::optional<CorsErrorStatus> PreflightResult::EnsureAllowedCrossOriginHeaders(
    const net::HttpRequestHeaders& headers,
    bool is_revalidating,
    NonWildcardRequestHeadersSupport non_wildcard_request_headers_support)
    const {
  const bool has_wildcard = !credentials_ && headers_.contains("*");
  if (has_wildcard) {
    if (non_wildcard_request_headers_support) {
      // "authorization" is the only member of
      // https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name.
      if (headers.HasHeader(kAuthorization) &&
          !headers_.contains(kAuthorization)) {
        CorsErrorStatus error_status(
            mojom::CorsError::kHeaderDisallowedByPreflightResponse,
            kAuthorization);
        error_status.has_authorization_covered_by_wildcard_on_preflight = true;
        return error_status;
      }
    }
    return std::nullopt;
  }

  // Forbidden headers are forbidden to be used by JavaScript, and checked
  // beforehand. But user-agents may add these headers internally, and it's
  // fine.
  for (const auto& name : CorsUnsafeNotForbiddenRequestHeaderNames(
           headers.GetHeaderVector(), is_revalidating)) {
    // Header list check is performed in case-insensitive way. Here, we have a
    // parsed header list set in lower case, and search each header in lower
    // case.
    if (!headers_.contains(name)) {
      return CorsErrorStatus(
          mojom::CorsError::kHeaderDisallowedByPreflightResponse, name);
    }
  }
  return std::nullopt;
}

bool PreflightResult::IsExpired() const {
  return absolute_expiry_time_ <= Now();
}

bool PreflightResult::EnsureAllowedRequest(
    mojom::CredentialsMode credentials_mode,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_revalidating,
    NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
    bool acam_preflight_spec_conformant) const {
  if (!credentials_ && credentials_mode == mojom::CredentialsMode::kInclude) {
    return false;
  }

  if (EnsureAllowedCrossOriginMethod(method, acam_preflight_spec_conformant)) {
    return false;
  }

  if (EnsureAllowedCrossOriginHeaders(headers, is_revalidating,
                                      non_wildcard_request_headers_support)) {
    return false;
  }

  return true;
}

std::optional<mojom::CorsError> PreflightResult::Parse(
    const std::optional<std::string>& allow_methods_header,
    const std::optional<std::string>& allow_headers_header,
    const std::optional<std::string>& max_age_header) {
  DCHECK(methods_.empty());
  DCHECK(headers_.empty());

  // Keeps parsed method case for case-sensitive search.
  if (!ParseAccessControlAllowList(allow_methods_header, &methods_, false))
    return mojom::CorsError::kInvalidAllowMethodsPreflightResponse;

  // Holds parsed headers in lower case for case-insensitive search.
  if (!ParseAccessControlAllowList(allow_headers_header, &headers_, true))
    return mojom::CorsError::kInvalidAllowHeadersPreflightResponse;

  const base::TimeDelta expiry_delta = ParseAccessControlMaxAge(max_age_header);
  absolute_expiry_time_ = Now() + expiry_delta;

  return std::nullopt;
}

bool PreflightResult::HasAuthorizationCoveredByWildcard(
    const net::HttpRequestHeaders& headers) const {
  // "*" acts as a wildcard symbol only when `credentials_` is false.
  const bool has_wildcard =
      !credentials_ && headers_.find("*") != headers_.end();

  return has_wildcard && headers.HasHeader(kAuthorization) &&
         !headers_.contains(kAuthorization);
}

base::Value::Dict PreflightResult::NetLogParams() const {
  return base::Value::Dict()
      .Set("access-control-allow-methods", JoinSet(methods_))
      .Set("access-control-allow-headers", JoinSet(headers_));
}

}  // namespace network::cors
