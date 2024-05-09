// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/quota/padding_key.h"

#include <inttypes.h>
#include <cstdint>
#include <vector>
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "crypto/symmetric_key.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using crypto::SymmetricKey;

namespace storage {

namespace {

const SymmetricKey::Algorithm kPaddingKeyAlgorithm = SymmetricKey::AES;

// The range of the padding added to response sizes for opaque resources.
// Increment the CacheStorage padding version if changed.
constexpr uint64_t kPaddingRange = 14431 * 1024;

std::unique_ptr<SymmetricKey>* GetPaddingKeyInternal() {
  static base::NoDestructor<std::unique_ptr<SymmetricKey>> s_padding_key([] {
    return SymmetricKey::GenerateRandomKey(kPaddingKeyAlgorithm, 128);
  }());
  return s_padding_key.get();
}

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
  DCHECK(!response_url.empty());

  net::SchemefulSite site(storage_key.origin());

  DCHECK_GT(response_time, base::Time::UnixEpoch());
  int64_t microseconds =
      (response_time - base::Time::UnixEpoch()).InMicroseconds();

  // It should only be possible to have a CORS safe-listed method here since
  // the spec does not permit other methods for no-cors requests.
  DCHECK(request_method == net::HttpRequestHeaders::kGetMethod ||
         request_method == net::HttpRequestHeaders::kHeadMethod ||
         request_method == net::HttpRequestHeaders::kPostMethod);

  std::string key = base::StringPrintf(
      "%s-%" PRId64 "-%s-%s-%" PRId64, response_url.c_str(), microseconds,
      site.Serialize().c_str(), request_method.c_str(), side_data_size);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  CHECK(hmac.Init(GetPaddingKeyInternal()->get()));

  uint64_t digest_start = 0;
  CHECK(hmac.Sign(key, reinterpret_cast<uint8_t*>(&digest_start),
                  sizeof(digest_start)));
  return digest_start % kPaddingRange;
}

}  // namespace storage
