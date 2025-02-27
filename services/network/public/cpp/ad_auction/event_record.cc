// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ad_auction/event_record.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

namespace {

constexpr std::string_view kAdAuctionRecordEventHeader =
    "Ad-Auction-Record-Event";

std::optional<std::string> ParseEventType(
    const net::structured_headers::Dictionary& dict) {
  const auto it = dict.find("type");
  if (it == dict.end()) {
    return std::nullopt;
  }
  const net::structured_headers::ParameterizedMember& parameterized_member =
      it->second;
  if (parameterized_member.member_is_inner_list) {
    return std::nullopt;
  }
  CHECK_EQ(parameterized_member.member.size(), 1u);
  const net::structured_headers::ParameterizedItem& parameterized_item =
      parameterized_member.member.front();
  if (!parameterized_item.item.is_string()) {
    return std::nullopt;
  }
  return parameterized_item.item.GetString();
}

std::optional<std::vector<url::Origin>> ParseEligibleOrigins(
    const net::structured_headers::Dictionary& dict) {
  const auto it = dict.find("eligible-origins");
  if (it == dict.end()) {
    // "eligible-origins" is optional, so just return an empty list.
    return {{}};
  }
  const net::structured_headers::ParameterizedMember& parameterized_member =
      it->second;
  if (!parameterized_member.member_is_inner_list) {
    return std::nullopt;
  }
  std::vector<url::Origin> result;
  for (const net::structured_headers::ParameterizedItem& parameterized_item :
       parameterized_member.member) {
    if (!parameterized_item.item.is_string()) {
      return std::nullopt;
    }
    result.emplace_back(
        url::Origin::Create(GURL(parameterized_item.item.GetString())));
  }
  return result;
}

}  // namespace

AdAuctionEventRecord::AdAuctionEventRecord() = default;
AdAuctionEventRecord::~AdAuctionEventRecord() = default;
AdAuctionEventRecord::AdAuctionEventRecord(AdAuctionEventRecord&&) = default;
AdAuctionEventRecord& AdAuctionEventRecord::operator=(AdAuctionEventRecord&&) =
    default;

// static
std::optional<std::string> AdAuctionEventRecord::GetAdAuctionRecordEventHeader(
    const net::HttpResponseHeaders* headers) {
  if (!headers) {
    return std::nullopt;
  }

  return headers->GetNormalizedHeader(kAdAuctionRecordEventHeader);
}

// static
std::optional<AdAuctionEventRecord>
AdAuctionEventRecord::MaybeCreateFromStructuredDict(
    const net::structured_headers::Dictionary& dict,
    Type expected_type,
    const url::Origin& providing_origin) {
  std::optional<std::string> event_type = ParseEventType(dict);
  if (!event_type) {
    return std::nullopt;
  }

  AdAuctionEventRecord result;
  result.providing_origin = providing_origin;
  switch (expected_type) {
    case Type::kUninitialized:
      NOTREACHED();
    case Type::kView:
      if (*event_type != "view") {
        return std::nullopt;
      }
      result.type = Type::kView;
      break;
    case Type::kClick:
      if (*event_type != "click") {
        return std::nullopt;
      }
      result.type = Type::kClick;
      break;
  }

  std::optional<std::vector<url::Origin>> eligible_origins =
      ParseEligibleOrigins(dict);
  if (!eligible_origins) {
    return std::nullopt;
  }
  result.eligible_origins = std::move(*eligible_origins);

  if (!result.IsValid()) {
    return std::nullopt;
  }

  return result;
}

bool AdAuctionEventRecord::IsValid() const {
  if (type == Type::kUninitialized) {
    return false;
  }
  if (providing_origin.scheme() != url::kHttpsScheme) {
    return false;
  }
  for (const url::Origin& origin : eligible_origins) {
    if (origin.scheme() != url::kHttpsScheme) {
      return false;
    }
  }
  return true;
}

}  // namespace network
