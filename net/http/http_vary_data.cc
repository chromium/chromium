// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_vary_data.h"

#include <array>
#include <string_view>
#include <variant>

#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "crypto/hash.h"
#include "crypto/obsolete/md5.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

crypto::obsolete::Md5 MakeMd5HasherForHttpVaryData() {
  return {};
}

namespace {

using HasherVariant = std::variant<crypto::obsolete::Md5, crypto::hash::Hasher>;

// Append to the given hash context for the given request header.
template <typename Hasher>
void AddField(const HttpRequestInfo& request_info,
              std::string_view request_header,
              Hasher& context) {
  std::string request_value =
      request_info.extra_headers.GetHeader(request_header)
          .value_or(std::string());

  // Append a character that cannot appear in the request header line so that we
  // protect against case where the concatenation of two request headers could
  // look the same for a variety of values for the individual request headers.
  // For example, "foo: 12\nbar: 3" looks like "foo: 1\nbar: 23" otherwise.
  request_value.append(1, '\n');
  context.Update(request_value);
}

// A helper to abstract away the hashing logic for different context types.
bool UpdateVaryContext(const HttpRequestInfo& request_info,
                       const HttpResponseHeaders& response_headers,
                       HasherVariant& context_variant) {
  bool processed_header = false;
  size_t iter = 0;
  constexpr std::string_view name = "vary";
  std::optional<std::string_view> request_header;
  while ((request_header = response_headers.EnumerateHeader(&iter, name))) {
    // The caller of this function is responsible for handling the "*" case.
    DCHECK_NE("*", *request_header);
    std::visit(
        [&](auto& context) {
          AddField(request_info, *request_header, context);
        },
        context_variant);
    processed_header = true;
  }
  return processed_header;
}
}  // namespace

HttpVaryData::HttpVaryData() = default;

bool HttpVaryData::Init(const HttpRequestInfo& request_info,
                        const HttpResponseHeaders& response_headers,
                        HashType hash_type) {
  is_valid_ = false;

  // If the Vary header contains '*' then we can just notice it based on
  // |cached_response_headers| in MatchesRequest(), and don't have to worry
  // about the specific headers.  We still want an HttpVaryData around, to let
  // us handle this case. See section 4.1 of RFC 7234.
  //

  if (response_headers.HasHeaderValue("vary", "*")) {
    // What's in request_digest_ will never be looked at, but make it
    // deterministic so we don't serialize out uninitialized memory content.
    hash_ = Sha256Hash{};
    return is_valid_ = true;
  }

  HasherVariant context_variant =
      (hash_type == HashType::kSHA256)
          ? HasherVariant(std::in_place_type<crypto::hash::Hasher>,
                          crypto::hash::HashKind::kSha256)
          : HasherVariant(MakeMd5HasherForHttpVaryData());

  if (!UpdateVaryContext(request_info, response_headers, context_variant)) {
    return false;
  }

  std::visit(
      [this](auto& context) {
        using T = std::decay_t<decltype(context)>;
        if constexpr (std::is_same_v<T, crypto::hash::Hasher>) {
          Sha256Hash sha_hash;
          context.Finish(sha_hash);
          hash_ = sha_hash;
        } else {
          Md5Hash md5_hash;
          context.Finish(md5_hash);
          hash_ = md5_hash;
        }
      },
      context_variant);

  return is_valid_ = true;
}

bool HttpVaryData::InitFromPickle(base::PickleIterator* iter) {
  is_valid_ = false;

  // Create a copy of the iterator to probe for the old format. This avoids
  // advancing `iter` before we know which format we are parsing.
  base::PickleIterator probe_iter(*iter);
  std::optional<base::span<const uint8_t>> bytes_for_probe =
      probe_iter.ReadBytes(crypto::obsolete::Md5::kSize);

  // Support for old cache entries that stored a raw 16-byte MD5 digest.
  // We detect 16-byte MD5 hash by checking if the pickle's total size is
  // exactly 16 bytes.
  if (bytes_for_probe && !probe_iter.ReadBytes(1).has_value()) {
    // It's an old-style entry. Read the data from the original iterator to
    // consume it.
    std::optional<base::span<const uint8_t>> bytes =
        iter->ReadBytes(crypto::obsolete::Md5::kSize);
    Md5Hash md5_hash;
    base::span(md5_hash).copy_from(*bytes);
    hash_ = md5_hash;
    return is_valid_ = true;
  }

  // It's a new-style entry, so we parse it from the original `iter`.
  // New format: HashType enum followed by the digest.
  int read_hash_type;
  if (!iter->ReadInt(&read_hash_type)) {
    return false;
  }

  if (read_hash_type == static_cast<int>(HashType::kSHA256)) {
    std::optional<base::span<const uint8_t>> bytes =
        iter->ReadBytes(crypto::hash::kSha256Size);
    if (!bytes) {
      return false;
    }
    Sha256Hash sha_hash;
    base::span(sha_hash).copy_from(*bytes);
    hash_ = sha_hash;
  } else if (read_hash_type == static_cast<int>(HashType::kMD5)) {
    std::optional<base::span<const uint8_t>> bytes =
        iter->ReadBytes(crypto::obsolete::Md5::kSize);
    if (!bytes) {
      return false;
    }
    Md5Hash md5_hash;
    base::span(md5_hash).copy_from(*bytes);
    hash_ = md5_hash;
  } else {
    return false;  // Invalid hash type.
  }

  return is_valid_ = true;
}

void HttpVaryData::Persist(base::Pickle* pickle) const {
  DCHECK(is_valid());
  std::visit(
      [&](const auto& hash) {
        pickle->WriteInt(static_cast<int>(hash_type()));
        pickle->WriteBytes(hash);
      },
      hash_);
}

bool HttpVaryData::MatchesRequest(
    const HttpRequestInfo& request_info,
    const HttpResponseHeaders& cached_response_headers) const {
  // Vary: * never matches.
  if (cached_response_headers.HasHeaderValue("vary", "*")) {
    return false;
  }

  HttpVaryData new_vary_data;
  if (!new_vary_data.Init(request_info, cached_response_headers, hash_type())) {
    // This case can happen if |this| was loaded from a cache that was populated
    // by a build before crbug.com/469675 was fixed.
    return false;
  }
  return new_vary_data.hash_ == hash_;
}

}  // namespace net
