// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/quota/padding_key.h"

#include <inttypes.h>

#include <cstdint>

#include "base/numerics/byte_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "crypto/hash.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

namespace {

// The range of the padding added to response sizes for opaque resources.
// Increment the CacheStorage padding version if changed.
constexpr uint64_t kPaddingRange = 14431 * 1024;

}  // namespace

bool ShouldPadResponseType(network::mojom::FetchResponseType type) {
  return type == network::mojom::FetchResponseType::kOpaque ||
         type == network::mojom::FetchResponseType::kOpaqueRedirect;
}

int64_t ComputeRandomResponsePadding() {
  uint64_t raw_random = 0;
  crypto::RandBytes(base::byte_span_from_ref(raw_random));
  return raw_random % kPaddingRange;
}

int64_t ComputeStableResponsePadding(const blink::StorageKey& storage_key,
                                     const std::string& response_url,
                                     const base::Time& response_time,
                                     const std::string& request_method,
                                     int64_t side_data_size) {
  // This just needs to be consistent within a single browser session, so we
  // generate it the first time we need it.
  static const std::array<uint8_t, 16> s_padding_key =
      crypto::RandBytesAsArray<16>();

  // Fall back to random padding for invalid or unexpected inputs rather than
  // DCHECKing: callers may pass persisted cache metadata or responses with
  // unset/pre-epoch timestamps or non-safelisted methods (e.g., opaque
  // redirects).
  if (response_time <= base::Time::UnixEpoch() ||
      (request_method != net::HttpRequestHeaders::kGetMethod &&
       request_method != net::HttpRequestHeaders::kHeadMethod &&
       request_method != net::HttpRequestHeaders::kPostMethod)) {
    return ComputeRandomResponsePadding();
  }

  GURL url(response_url);
  if (!url.is_valid()) {
    return ComputeRandomResponsePadding();
  }
  const GURL simplified_url = net::SimplifyUrlForRequest(url);
  DCHECK(simplified_url.is_valid());

  net::SchemefulSite site(storage_key.origin());
  int64_t microseconds =
      (response_time - base::Time::UnixEpoch()).InMicroseconds();

  std::string key = base::StringPrintf("%s-%" PRId64 "-%s-%s-%" PRId64,
                                       simplified_url.spec().c_str(),
                                       microseconds, site.Serialize().c_str(),
                                       request_method.c_str(), side_data_size);

  auto hmac = crypto::hmac::SignSha256(s_padding_key, base::as_byte_span(key));
  return base::U64FromNativeEndian(base::as_byte_span(hmac).first<8>()) %
         kPaddingRange;
}

}  // namespace storage
