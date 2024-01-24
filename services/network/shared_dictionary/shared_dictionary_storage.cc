// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage.h"

#include <algorithm>
#include <string_view>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace network {

namespace {

constexpr std::string_view kDefaultTypeRaw = "raw";

class UseAsDictionaryHeaderInfo {
 public:
  UseAsDictionaryHeaderInfo(std::string match,
                            absl::optional<base::TimeDelta> expiration,
                            std::string type)
      : match(std::move(match)),
        expiration(expiration),
        type(std::move(type)) {}
  ~UseAsDictionaryHeaderInfo() = default;

  std::string match;
  absl::optional<base::TimeDelta> expiration;
  std::string type;
};

absl::optional<UseAsDictionaryHeaderInfo> ParseUseAsDictionaryHeaderInfo(
    const net::HttpResponseHeaders& headers) {
  std::string use_as_dictionary_header;
  if (!headers.GetNormalizedHeader(
          shared_dictionary::kUseAsDictionaryHeaderName,
          &use_as_dictionary_header)) {
    return absl::nullopt;
  }
  absl::optional<net::structured_headers::Dictionary> dictionary =
      net::structured_headers::ParseDictionary(use_as_dictionary_header);
  if (!dictionary) {
    return absl::nullopt;
  }

  absl::optional<std::string> match_value;
  absl::optional<base::TimeDelta> expires_value;
  std::string type_value = std::string(kDefaultTypeRaw);
  for (const auto& entry : dictionary.value()) {
    if (entry.first == shared_dictionary::kOptionNameMatch) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_string()) {
        return absl::nullopt;
      }
      match_value = entry.second.member.front().item.GetString();
    } else if (entry.first == shared_dictionary::kOptionNameExpires) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_integer()) {
        return absl::nullopt;
      }
      expires_value =
          base::Seconds(entry.second.member.front().item.GetInteger());
    } else if (entry.first == shared_dictionary::kOptionNameType) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_token()) {
        return absl::nullopt;
      }
      type_value = entry.second.member.front().item.GetString();
    }
  }
  if (!match_value) {
    return absl::nullopt;
  }
  return UseAsDictionaryHeaderInfo(*match_value, std::move(expires_value),
                                   type_value);
}

}  // namespace

SharedDictionaryStorage::SharedDictionaryStorage() = default;

SharedDictionaryStorage::~SharedDictionaryStorage() = default;

scoped_refptr<SharedDictionaryWriter>
SharedDictionaryStorage::MaybeCreateWriter(
    const GURL& url,
    base::Time response_time,
    const net::HttpResponseHeaders& headers,
    bool was_fetched_via_cache,
    base::OnceCallback<bool()> access_allowed_check_callback) {
  absl::optional<UseAsDictionaryHeaderInfo> info =
      ParseUseAsDictionaryHeaderInfo(headers);
  if (!info) {
    return nullptr;
  }
  base::TimeDelta expiration = shared_dictionary::kDefaultExpiration;
  if (info->expiration) {
    expiration = *info->expiration;
  }
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
      IsAlreadyRegistered(url, response_time, expiration, info->match)) {
    return nullptr;
  }

  if (!std::move(access_allowed_check_callback).Run()) {
    return nullptr;
  }

  return CreateWriter(url, response_time, expiration, info->match);
}

}  // namespace network
