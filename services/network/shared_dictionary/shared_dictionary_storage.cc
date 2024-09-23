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
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/shared_dictionary_error.mojom.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "services/network/shared_dictionary/simple_url_pattern_matcher.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace network {

namespace {

constexpr std::string_view kDefaultTypeRaw = "raw";

class DictionaryHeaderInfo {
 public:
  DictionaryHeaderInfo(std::string match,
                       std::set<network::mojom::RequestDestination> match_dest,
                       std::string type,
                       std::string id)
      : match(std::move(match)),
        match_dest(std::move(match_dest)),
        type(std::move(type)),
        id(std::move(id)) {}
  ~DictionaryHeaderInfo() = default;

  std::string match;
  std::set<network::mojom::RequestDestination> match_dest;
  std::string type;
  std::string id;
};

base::TimeDelta CalculateExpiration(const net::HttpResponseHeaders& headers,
                                    const base::Time request_time,
                                    const base::Time response_time) {
  // Use the freshness lifetime calculated from the response header.
  net::HttpResponseHeaders::FreshnessLifetimes lifetimes =
      headers.GetFreshnessLifetimes(response_time);
  // We calculate `expires_value` which is a delta from the response time to
  // the expiration time. So we get the age of the response on the response
  // time by setting `current_time` argument to `response_time`.
  base::TimeDelta age_on_response_time =
      headers.GetCurrentAge(request_time, response_time,
                            /*current_time=*/response_time);
  // We can use `freshness + staleness - current_age` as the expiration time.
  return lifetimes.freshness + lifetimes.staleness - age_on_response_time;
}

base::expected<DictionaryHeaderInfo, mojom::SharedDictionaryError>
ParseDictionaryHeaderInfo(const std::string& use_as_dictionary_header) {
  std::optional<net::structured_headers::Dictionary> dictionary =
      net::structured_headers::ParseDictionary(use_as_dictionary_header);
  if (!dictionary) {
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorInvalidStructuredHeader);
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
        return base::unexpected(
            mojom::SharedDictionaryError::kWriteErrorNonStringMatchField);
      }
      match_value = entry.second.member.front().item.GetString();
    } else if (entry.first == shared_dictionary::kOptionNameMatchDest) {
      if (!entry.second.member_is_inner_list) {
        // `match-dest` must be a list.
        return base::unexpected(
            mojom::SharedDictionaryError::kWriteErrorNonListMatchDestField);
      }
      for (const auto& item : entry.second.member) {
        if (!item.item.is_string()) {
          return base::unexpected(mojom::SharedDictionaryError::
                                      kWriteErrorNonStringInMatchDestList);
        }
        // We use the empty string "" for RequestDestination::kEmpty in
        // `match-dest`.
        std::optional<mojom::RequestDestination> dest_value =
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
        return base::unexpected(
            mojom::SharedDictionaryError::kWriteErrorNonTokenTypeField);
      }
      type_value = entry.second.member.front().item.GetString();
    } else if (entry.first == shared_dictionary::kOptionNameId) {
      if ((entry.second.member.size() != 1u) ||
          !entry.second.member.front().item.is_string()) {
        return base::unexpected(
            mojom::SharedDictionaryError::kWriteErrorNonStringIdField);
      }
      id_value = entry.second.member.front().item.GetString();
      if (id_value.size() > shared_dictionary::kDictionaryIdMaxLength) {
        return base::unexpected(
            mojom::SharedDictionaryError::kWriteErrorTooLongIdField);
      }
    }
  }
  if (!match_value) {
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorNoMatchField);
  }

  return DictionaryHeaderInfo(std::move(*match_value),
                              std::move(match_dest_values),
                              std::move(type_value), std::move(id_value));
}

}  // namespace

SharedDictionaryStorage::SharedDictionaryStorage() = default;

SharedDictionaryStorage::~SharedDictionaryStorage() = default;

// static
base::expected<scoped_refptr<SharedDictionaryWriter>,
               mojom::SharedDictionaryError>
SharedDictionaryStorage::MaybeCreateWriter(
    const std::string& use_as_dictionary_header,
    bool shared_dictionary_writer_enabled,
    SharedDictionaryStorage* storage,
    mojom::RequestMode request_mode,
    mojom::FetchResponseType response_tainting,
    const GURL& url,
    const base::Time request_time,
    const base::Time response_time,
    const net::HttpResponseHeaders& headers,
    bool was_fetched_via_cache,
    base::OnceCallback<bool()> access_allowed_check_callback) {
  // Supports storing dictionaries if the request was fetched by cors enabled
  // mode request or same-origin mode request or no-cors mode same origin
  // request.
  switch (request_mode) {
    case mojom::RequestMode::kSameOrigin:
      break;
    case mojom::RequestMode::kNoCors:
      // Basic `response_tainting` for no-cors request means that the response
      // is from same origin without any cross origin redirect.
      if (response_tainting != mojom::FetchResponseType::kBasic) {
        return base::unexpected(
            mojom::SharedDictionaryError::kWriteErrorCossOriginNoCorsRequest);
      }
      break;
    case mojom::RequestMode::kCors:
      break;
    case mojom::RequestMode::kCorsWithForcedPreflight:
      break;
    case mojom::RequestMode::kNavigate:
      return base::unexpected(
          mojom::SharedDictionaryError::kWriteErrorNavigationRequest);
  }
  if (!shared_dictionary_writer_enabled) {
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorFeatureDisabled);
  }
  if (!storage) {
    // CorsURLLoader passes a null `storage`, when the request is not from
    // secure context.
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorNonSecureContext);
  }
  // Opaque response tainting requests should not trigger dictionary
  // registration.
  CHECK_NE(mojom::FetchResponseType::kOpaque, response_tainting);

  base::TimeDelta expiration =
      CalculateExpiration(headers, request_time, response_time);
  if (expiration <= base::TimeDelta()) {
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorExpiredResponse);
  }
  if (!base::FeatureList::IsEnabled(
          network::features::kCompressionDictionaryTransport)) {
    // During the Origin Trial experiment, kCompressionDictionaryTransport is
    // disabled in the network service. In that case, we have a maximum
    // expiration time on the dictionary entry to keep the duration constrained.
    expiration =
        std::min(expiration, shared_dictionary::kMaxExpirationForOriginTrial);
  }

  base::expected<DictionaryHeaderInfo, mojom::SharedDictionaryError> info =
      ParseDictionaryHeaderInfo(use_as_dictionary_header);
  if (!info.has_value()) {
    return base::unexpected(info.error());
  }
  if (info->type != kDefaultTypeRaw) {
    // Currently we only support `raw` type.
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorUnsupportedType);
  }
  base::Time last_fetch_time = base::Time::Now();
  // Do not write an existing shared dictionary from the HTTP caches to the
  // shared dictionary storage. Note that IsAlreadyRegistered() can return false
  // even when `was_fetched_via_cache` is true. This is because the shared
  // dictionary storage has its own cache eviction logic, which is different
  // from the HTTP Caches's eviction logic.
  if (was_fetched_via_cache &&
      storage->UpdateLastFetchTimeIfAlreadyRegistered(
          url, response_time, expiration, info->match, info->match_dest,
          info->id, last_fetch_time)) {
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorAlreadyRegistered);
  }

  if (!std::move(access_allowed_check_callback).Run()) {
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorDisallowedBySettings);
  }

  auto matcher_create_result =
      SimpleUrlPatternMatcher::Create(info->match, url);
  if (!matcher_create_result.has_value()) {
    return base::unexpected(
        mojom::SharedDictionaryError::kWriteErrorInvalidMatchField);
  }

  return storage->CreateWriter(url, last_fetch_time, response_time, expiration,
                               info->match, info->match_dest, info->id,
                               std::move(matcher_create_result.value()));
}

}  // namespace network
