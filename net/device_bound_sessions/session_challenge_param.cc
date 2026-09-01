// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_challenge_param.h"

#include <algorithm>

#include "net/base/features.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace {
// Secure-Session-Challenge header defined in
// https://github.com/WICG/dbsc/blob/main/README.md#high-level-overview
constexpr char kSessionChallengeHeaderName[] = "Secure-Session-Challenge";

constexpr char kSessionIdKey[] = "id";
}  // namespace

namespace net::device_bound_sessions {

SessionChallengeParam::SessionChallengeParam(
    SessionChallengeParam&& other) noexcept = default;

SessionChallengeParam& SessionChallengeParam::operator=(
    SessionChallengeParam&& other) noexcept = default;

SessionChallengeParam::~SessionChallengeParam() = default;

SessionChallengeParam::SessionChallengeParam(
    std::optional<std::string> session_id,
    std::string challenge)
    : session_id_(std::move(session_id)), challenge_(std::move(challenge)) {}

// static
std::optional<SessionChallengeParam> SessionChallengeParam::ParseItem(
    const structured_headers::ParameterizedMember& session_challenge) {
  const auto item_and_params = session_challenge.GetWithParamsIfItem();
  if (!item_and_params.has_value()) {
    return std::nullopt;
  }

  const std::string* challenge = item_and_params->first.GetIfString();
  if (!challenge || challenge->empty()) {
    return std::nullopt;
  }

  std::optional<std::string> session_id;
  if (auto it = std::ranges::find(
          item_and_params->second, kSessionIdKey,
          &std::pair<std::string, structured_headers::Item>::first);
      it != item_and_params->second.end()) {
    const std::string* string = it->second.GetIfString();
    if (!string) {
      return std::nullopt;
    }

    if (!string->empty()) {
      session_id = *string;
    }
  }

  return SessionChallengeParam(std::move(session_id), *challenge);
}

// static
std::vector<SessionChallengeParam> SessionChallengeParam::CreateIfValid(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers) {
  std::vector<SessionChallengeParam> params;
  if (!request_url.is_valid()) {
    return params;
  }

  if (!headers) {
    return params;
  }
  std::optional<std::string> header_value =
      headers->GetNormalizedHeader(kSessionChallengeHeaderName);
  if (!header_value) {
    return params;
  }

  std::optional<structured_headers::List> list =
      structured_headers::ParseList(*header_value);

  if (!list) {
    return params;
  }

  for (const auto& session_challenge : *list) {
    std::optional<SessionChallengeParam> param = ParseItem(session_challenge);
    if (param) {
      params.push_back(std::move(*param));
    }
  }

  return params;
}

}  // namespace net::device_bound_sessions
