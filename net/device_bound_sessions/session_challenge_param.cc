// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_challenge_param.h"

#include "base/ranges/algorithm.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace {
// Sec-Session-Challenge header defined in
// https://github.com/WICG/dbsc/blob/main/README.md#high-level-overview
constexpr char kSessionChallengeHeaderName[] = "Sec-Session-Challenge";
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
  if (session_challenge.member_is_inner_list ||
      session_challenge.member.empty()) {
    return std::nullopt;
  }

  const structured_headers::Item& item = session_challenge.member[0].item;
  if (!item.is_string()) {
    return std::nullopt;
  }

  std::string challenge(item.GetString());
  if (challenge.empty()) {
    return std::nullopt;
  }

  std::optional<std::string> session_id;
  if (auto it = base::ranges::find(
          session_challenge.params, kSessionIdKey,
          &std::pair<std::string, structured_headers::Item>::first);
      it != session_challenge.params.end()) {
    const auto& param = it->second;
    if (!param.is_string()) {
      return std::nullopt;
    }

    auto id = param.GetString();
    if (!id.empty()) {
      session_id = std::move(id);
    }
  }

  return SessionChallengeParam(std::move(session_id), std::move(challenge));
}

// static
std::vector<SessionChallengeParam> SessionChallengeParam::CreateIfValid(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers) {
  std::vector<SessionChallengeParam> params;
  if (!request_url.is_valid()) {
    return params;
  }

  std::string header_value;
  if (!headers || !headers->GetNormalizedHeader(kSessionChallengeHeaderName,
                                                &header_value)) {
    return params;
  }

  std::optional<structured_headers::List> list =
      structured_headers::ParseList(header_value);

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
