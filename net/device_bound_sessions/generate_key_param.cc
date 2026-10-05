// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/generate_key_param.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "net/device_bound_sessions/session_binding_utils.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

namespace {

constexpr std::string_view kTargetOriginParamKey = "target_origin";
constexpr std::string_view kProviderSessionIdParamKey = "provider_session_id";
constexpr std::string_view kChallengeParamKey = "challenge";

}  // namespace

// static
std::optional<GenerateKeyParam> GenerateKeyParam::Parse(
    std::string_view header_value) {
  ASSIGN_OR_RETURN(const structured_headers::List list,
                   structured_headers::ParseListStrict(header_value));
  if (list.size() != 1) {
    return std::nullopt;
  }

  const structured_headers::InnerList* inner_list = list[0].GetIfInnerList();
  if (!inner_list) {
    return std::nullopt;
  }

  std::vector<crypto::sign::SignatureKind> supported_algos =
      ParseSupportedAlgorithms(inner_list->items);
  if (supported_algos.empty()) {
    return std::nullopt;
  }

  // Absent and empty parameters are treated the same and rejected below. An
  // empty `target_origin` yields an opaque origin.
  std::string target_origin;
  std::string provider_session_id;
  std::string challenge;
  for (const auto& [key, value] : inner_list->params) {
    // Quiche collapses duplicate keys, keeping the last value. Known keys with
    // a non-string value reject the whole header.
    if (key == kTargetOriginParamKey) {
      ASSIGN_OR_RETURN(target_origin,
                       base::OptionalFromPtr(value.GetIfString()));
    } else if (key == kProviderSessionIdParamKey) {
      ASSIGN_OR_RETURN(provider_session_id,
                       base::OptionalFromPtr(value.GetIfString()));
    } else if (key == kChallengeParamKey) {
      ASSIGN_OR_RETURN(challenge, base::OptionalFromPtr(value.GetIfString()));
    }

    // Other params are ignored.
  }

  if (provider_session_id.empty() || challenge.empty()) {
    return std::nullopt;
  }

  url::Origin origin = url::Origin::Create(GURL(target_origin));
  if (origin.opaque() || !IsSecure(origin.GetURL())) {
    return std::nullopt;
  }

  return GenerateKeyParam{
      .supported_algos = std::move(supported_algos),
      .target_origin = std::move(origin),
      .provider_session_id = Session::Id(std::move(provider_session_id)),
      .challenge = std::move(challenge),
  };
}

}  // namespace net::device_bound_sessions
