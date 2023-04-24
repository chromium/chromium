// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage.h"

#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace network {

namespace {

class UseAsDictionaryHeaderInfo {
 public:
  UseAsDictionaryHeaderInfo(std::string path_pattern,
                            absl::optional<int64_t> expiration,
                            absl::optional<std::vector<std::string>> hashes)
      : path_pattern(std::move(path_pattern)),
        expiration(expiration),
        hashes(std::move(hashes)) {}
  ~UseAsDictionaryHeaderInfo() = default;

  std::string path_pattern;
  absl::optional<int64_t> expiration;
  absl::optional<std::vector<std::string>> hashes;
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

  absl::optional<std::string> p_value;
  absl::optional<int64_t> e_value;
  absl::optional<std::vector<std::string>> h_value;
  for (const auto& entry : dictionary.value()) {
    if (entry.first == shared_dictionary::kOptionNameMatch) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_string()) {
        return absl::nullopt;
      }
      p_value = entry.second.member.front().item.GetString();
    } else if (entry.first == shared_dictionary::kOptionNameExpires) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_integer()) {
        return absl::nullopt;
      }
      e_value = entry.second.member.front().item.GetInteger();
    } else if (entry.first == shared_dictionary::kOptionNameAlgorithms) {
      std::vector<std::string> tmp_vec;
      for (const auto& h_item : entry.second.member) {
        if (!h_item.item.is_token()) {
          return absl::nullopt;
        }
        tmp_vec.push_back(h_item.item.GetString());
      }
      h_value = std::move(tmp_vec);
    }
  }
  if (!p_value) {
    return absl::nullopt;
  }
  return UseAsDictionaryHeaderInfo(*p_value, std::move(e_value),
                                   std::move(h_value));
}

}  // namespace

SharedDictionaryStorage::SharedDictionaryStorage() = default;

SharedDictionaryStorage::~SharedDictionaryStorage() = default;

scoped_refptr<SharedDictionaryWriter>
SharedDictionaryStorage::MaybeCreateWriter(
    const GURL& url,
    base::Time response_time,
    const net::HttpResponseHeaders& headers) {
  absl::optional<UseAsDictionaryHeaderInfo> info =
      ParseUseAsDictionaryHeaderInfo(headers);
  if (!info) {
    return nullptr;
  }
  int64_t expiration = shared_dictionary::kDefaultExpiration;
  if (info->expiration) {
    expiration = *info->expiration;
  }
  if (info->hashes) {
    // Currently we only support support sha-256.
    // TODO(crbug.com/1413922): Investigate the spec and decide whether to
    // support non lowercase token or not.
    if (std::find(info->hashes->begin(), info->hashes->end(), "sha-256") ==
        info->hashes->end()) {
      return nullptr;
    }
  }

  return CreateWriter(url, response_time, expiration, info->path_pattern);
}

}  // namespace network
