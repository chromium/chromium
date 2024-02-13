// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace network {

namespace {

constexpr std::string_view kDefaultTypeRaw = "raw";

class DictionaryHeaderInfo {
 public:
  DictionaryHeaderInfo(std::string match,
                       std::set<network::mojom::RequestDestination> match_dest,
                       base::TimeDelta expiration,
                       std::string type,
                       std::string id)
      : match(std::move(match)),
        match_dest(std::move(match_dest)),
        expiration(expiration),
        type(std::move(type)),
        id(std::move(id)) {}
  ~DictionaryHeaderInfo() = default;

  std::string match;
  std::set<network::mojom::RequestDestination> match_dest;
  base::TimeDelta expiration;
  std::string type;
  std::string id;
};

std::optional<DictionaryHeaderInfo> ParseDictionaryHeaderInfo(
    const net::HttpResponseHeaders& headers,
    const base::Time request_time,
    const base::Time response_time) {
  std::string use_as_dictionary_header;
  if (!headers.GetNormalizedHeader(
          shared_dictionary::kUseAsDictionaryHeaderName,
          &use_as_dictionary_header)) {
    return std::nullopt;
  }
  std::optional<net::structured_headers::Dictionary> dictionary =
      net::structured_headers::ParseDictionary(use_as_dictionary_header);
  if (!dictionary) {
    return std::nullopt;
  }

  std::optional<std::string> match_value;
  // Maybe we don't need to support multiple match-dest.
  // https://github.com/httpwg/http-extensions/issues/2722
  std::set<network::mojom::RequestDestination> match_dest_values;
  std::string type_value = std::string(kDefaultTypeRaw);
  std::string id_value;
  for (const auto& entry : dictionary.value()) {
    if (entry.first == shared_dictionary::kOptionNameMatch) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_string()) {
        return std::nullopt;
      }
      match_value = entry.second.member.front().item.GetString();
    } else if (entry.first == shared_dictionary::kOptionNameMatchDest) {
      if (!entry.second.member_is_inner_list) {
        // `match-dest` must be a list.
        return std::nullopt;
      }
      for (const auto& item : entry.second.member) {
        if (!item.item.is_string()) {
          return std::nullopt;
        }
        // We use the empty string "" for RequestDestination::kEmpty in
        // `match-dest`.
        std::optional<network::mojom::RequestDestination> dest_value =
            RequestDestinationFromString(
                item.item.GetString(),
                EmptyRequestDestinationOption::kUseTheEmptyString);
        if (dest_value) {
          match_dest_values.insert(*dest_value);
        }
      }
    } else if (entry.first == shared_dictionary::kOptionNameType) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_token()) {
        return std::nullopt;
      }
      type_value = entry.second.member.front().item.GetString();
    } else if (entry.first == shared_dictionary::kOptionNameId) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_string()) {
        return std::nullopt;
      }
      id_value = entry.second.member.front().item.GetString();
      if (id_value.size() > shared_dictionary::kDictionaryIdMaxLength) {
        return std::nullopt;
      }
    }
  }
  if (!match_value) {
    return std::nullopt;
  }

  // Use the fressness lifetime caliculated from the response header.
  net::HttpResponseHeaders::FreshnessLifetimes lifetimes =
      headers.GetFreshnessLifetimes(response_time);
  // We calculate `expires_value` which is a delta from the response time to
  // the expiration time. So we get the age of the response on the response
  // time by setting `current_time` argument to `response_time`.
  base::TimeDelta age_on_response_time =
      headers.GetCurrentAge(request_time, response_time,
                            /*current_time=*/response_time);
  // We can use `freshness + staleness - current_age` as the expiration time.
  base::TimeDelta expiration =
      lifetimes.freshness + lifetimes.staleness - age_on_response_time;
  if (expiration <= base::TimeDelta()) {
    return std::nullopt;
  }

  return DictionaryHeaderInfo(std::move(*match_value),
                              std::move(match_dest_values), expiration,
                              std::move(type_value), std::move(id_value));
}

}  // namespace

SharedDictionaryStorage::SharedDictionaryStorage() = default;

SharedDictionaryStorage::~SharedDictionaryStorage() = default;

scoped_refptr<SharedDictionaryWriter>
SharedDictionaryStorage::MaybeCreateWriter(
    const GURL& url,
    const base::Time request_time,
    const base::Time response_time,
    const net::HttpResponseHeaders& headers,
    bool was_fetched_via_cache,
    base::OnceCallback<bool()> access_allowed_check_callback) {
  std::optional<DictionaryHeaderInfo> info =
      ParseDictionaryHeaderInfo(headers, request_time, response_time);
  if (!info) {
    return nullptr;
  }
  base::TimeDelta expiration = info->expiration;
  if (!base::FeatureList::IsEnabled(
          network::features::kCompressionDictionaryTransport)) {
    // During the Origin Trial experiment, kCompressionDictionaryTransport is
    // disabled in the network service. In that case, we have a maximum
    // expiration time on the dictionary entry to keep the duration constrained.
    expiration =
        std::min(expiration, shared_dictionary::kMaxExpirationForOriginTrial);
  }
  if (info->type != kDefaultTypeRaw) {
    // Currently we only support `raw` type.
    return nullptr;
  }
  // Do not write an existing shared dictionary from the HTTP caches to the
  // shared dictionary storage. Note that IsAlreadyRegistered() can return false
  // even when `was_fetched_via_cache` is true. This is because the shared
  // dictionary storage has its own cache eviction logic, which is different
  // from the HTTP Caches's eviction logic.
  if (was_fetched_via_cache &&
      IsAlreadyRegistered(url, response_time, expiration, info->match,
                          info->match_dest, info->id)) {
    return nullptr;
  }

  if (!std::move(access_allowed_check_callback).Run()) {
    return nullptr;
  }

  return CreateWriter(url, response_time, expiration, info->match,
                      info->match_dest, info->id);
}

}  // namespace network
