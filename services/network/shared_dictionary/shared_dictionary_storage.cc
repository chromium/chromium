// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage.h"

#include <algorithm>

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

class UseAsDictionaryHeaderInfo {
 public:
  UseAsDictionaryHeaderInfo(std::string match,
                            absl::optional<base::TimeDelta> expiration,
                            absl::optional<std::vector<std::string>> algorithms)
      : match(std::move(match)),
        expiration(expiration),
        algorithms(std::move(algorithms)) {}
  ~UseAsDictionaryHeaderInfo() = default;

  std::string match;
  absl::optional<base::TimeDelta> expiration;
  absl::optional<std::vector<std::string>> algorithms;
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
  absl::optional<std::vector<std::string>> algorithms_value;
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
    } else if (entry.first == shared_dictionary::kOptionNameAlgorithms) {
      std::vector<std::string> tmp_vec;
      for (const auto& algorithms_item : entry.second.member) {
        if (!algorithms_item.item.is_token()) {
          return absl::nullopt;
        }
        tmp_vec.push_back(algorithms_item.item.GetString());
      }
      algorithms_value = std::move(tmp_vec);
    }
  }
  if (!match_value) {
    return absl::nullopt;
  }
  return UseAsDictionaryHeaderInfo(*match_value, std::move(expires_value),
                                   std::move(algorithms_value));
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
  if (info->algorithms) {
    // Currently we only support support sha-256.
    // TODO(crbug.com/1413922): Investigate the spec and decide whether to
    // support non lowercase token or not.
    if (std::find(info->algorithms->begin(), info->algorithms->end(),
                  "sha-256") == info->algorithms->end()) {
      return nullptr;
    }
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
