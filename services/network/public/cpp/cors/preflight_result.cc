// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/preflight_result.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/cors/cors.h"

namespace network {

namespace cors {

namespace {

// Timeout values below are at the discretion of the user agent.

// Default cache expiry time for an entry that does not have
// Access-Control-Max-Age header in its CORS-preflight response.
constexpr base::TimeDelta kDefaultTimeout = base::TimeDelta::FromSeconds(5);

// Maximum cache expiry time. Even if a CORS-preflight response contains
// Access-Control-Max-Age header that specifies a longer expiry time, this
// maximum time is applied.
constexpr base::TimeDelta kMaxTimeout = base::TimeDelta::FromHours(2);

// Holds TickClock instance to overwrite TimeTicks::Now() for testing.
const base::TickClock* tick_clock_for_testing = nullptr;

base::TimeTicks Now() {
  if (tick_clock_for_testing)
    return tick_clock_for_testing->NowTicks();
  return base::TimeTicks::Now();
}

bool ParseAccessControlMaxAge(const base::Optional<std::string>& max_age,
                              base::TimeDelta* expiry_delta) {
  DCHECK(expiry_delta);

  if (!max_age)
    return false;

  uint64_t delta;
  if (!base::StringToUint64(*max_age, &delta))
    return false;

  *expiry_delta = base::TimeDelta::FromSeconds(delta);
  if (*expiry_delta > kMaxTimeout)
    *expiry_delta = kMaxTimeout;
  return true;
}

// At this moment, this function always succeeds.
bool ParseAccessControlAllowList(const base::Optional<std::string>& string,
                                 base::flat_set<std::string>* set,
                                 bool insert_in_lower_case) {
  DCHECK(set);

  if (!string)
    return true;

  for (const auto& value : base::SplitString(
           *string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    // TODO(toyoshim): Strict ABNF header field checks want to be applied, e.g.
    // strict VCHAR check of RFC-7230.
    set->insert(insert_in_lower_case ? base::ToLowerASCII(value) : value);
  }
  return true;
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
    const base::Optional<std::string>& allow_methods_header,
    const base::Optional<std::string>& allow_headers_header,
    const base::Optional<std::string>& max_age_header,
    base::Optional<mojom::CorsError>* detected_error) {
  std::unique_ptr<PreflightResult> result =
      base::WrapUnique(new PreflightResult(credentials_mode));
  base::Optional<mojom::CorsError> error =
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

base::Optional<CorsErrorStatus> PreflightResult::EnsureAllowedCrossOriginMethod(
    const std::string& method) const {
  // Request method is normalized to upper case, and comparison is performed in
  // case-sensitive way, that means access control header should provide an
  // upper case method list.
  const std::string normalized_method = base::ToUpperASCII(method);
  if (methods_.find(normalized_method) != methods_.end() ||
      IsCorsSafelistedMethod(normalized_method)) {
    return base::nullopt;
  }

  if (!credentials_ && methods_.find("*") != methods_.end())
    return base::nullopt;

  return CorsErrorStatus(mojom::CorsError::kMethodDisallowedByPreflightResponse,
                         method);
}

base::Optional<CorsErrorStatus>
PreflightResult::EnsureAllowedCrossOriginHeaders(
    const net::HttpRequestHeaders& headers,
    bool is_revalidating,
    const base::flat_set<std::string>& extra_safelisted_header_names) const {
  if (!credentials_ && headers_.find("*") != headers_.end())
    return base::nullopt;

  // Forbidden headers are forbidden to be used by JavaScript, and checked
  // beforehand. But user-agents may add these headers internally, and it's
  // fine.
  for (const auto& name : CorsUnsafeNotForbiddenRequestHeaderNames(
           headers.GetHeaderVector(), is_revalidating,
           extra_safelisted_header_names)) {
    // Header list check is performed in case-insensitive way. Here, we have a
    // parsed header list set in lower case, and search each header in lower
    // case.
    if (headers_.find(name) == headers_.end()) {
      return CorsErrorStatus(
          mojom::CorsError::kHeaderDisallowedByPreflightResponse, name);
    }
  }
  return base::nullopt;
}

bool PreflightResult::IsExpired() const {
  return absolute_expiry_time_ <= Now();
}

bool PreflightResult::EnsureAllowedRequest(
    mojom::CredentialsMode credentials_mode,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_revalidating) const {
  if (!credentials_ && credentials_mode == mojom::CredentialsMode::kInclude) {
    return false;
  }

  if (EnsureAllowedCrossOriginMethod(method))
    return false;

  if (EnsureAllowedCrossOriginHeaders(headers, is_revalidating))
    return false;

  return true;
}

base::Optional<mojom::CorsError> PreflightResult::Parse(
    const base::Optional<std::string>& allow_methods_header,
    const base::Optional<std::string>& allow_headers_header,
    const base::Optional<std::string>& max_age_header) {
  DCHECK(methods_.empty());
  DCHECK(headers_.empty());

  // Keeps parsed method case for case-sensitive search.
  if (!ParseAccessControlAllowList(allow_methods_header, &methods_, false))
    return mojom::CorsError::kInvalidAllowMethodsPreflightResponse;

  // Holds parsed headers in lower case for case-insensitive search.
  if (!ParseAccessControlAllowList(allow_headers_header, &headers_, true))
    return mojom::CorsError::kInvalidAllowHeadersPreflightResponse;

  base::TimeDelta expiry_delta;
  if (max_age_header) {
    // Set expiry_delta to 0 on invalid Access-Control-Max-Age headers so to
    // invalidate the entry immediately. CORS-preflight response should be still
    // usable for the request that initiates the CORS-preflight.
    if (!ParseAccessControlMaxAge(max_age_header, &expiry_delta))
      expiry_delta = base::TimeDelta();
  } else {
    expiry_delta = kDefaultTimeout;
  }
  absolute_expiry_time_ = Now() + expiry_delta;

  return base::nullopt;
}

}  // namespace cors

}  // namespace network
