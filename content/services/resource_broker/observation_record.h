// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_RESOURCE_BROKER_OBSERVATION_RECORD_H_
#define CONTENT_SERVICES_RESOURCE_BROKER_OBSERVATION_RECORD_H_

#include <array>
#include <cstdint>

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "crypto/hash.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace content {

enum class ResponseSource {
  kNetwork,
  kCache,
  kMaxValue = kCache,
};

// Coarse MIME-type classification for recorded responses.
enum class MimeFamily {
  kOther,
  kScript,
  kCss,
  kFont,
  kImage,
  kMaxValue = kImage,
};

// Metadata and timing recorded for a completed response body observed by the
// broker.
struct ObservationRecord {
  std::array<uint8_t, crypto::hash::kSha256Size> body_hash = {};
  uint64_t body_size = 0;

  network::mojom::RequestDestination destination =
      network::mojom::RequestDestination::kEmpty;
  MimeFamily mime_family = MimeFamily::kOther;
  // Network isolation partition of the factory that loaded this response.
  base::UnguessableToken partition_key;
  // `base::FastHash()` of the final response URL with the fragment stripped,
  // used to distinguish same-URL duplicates from cross-URL duplicates.
  uint64_t url_hash = 0;
  // Bitmask of `EligibilityFailureReason` values, or `kEligible` (0) when the
  // response is eligible for deduplication.
  uint32_t ineligibility_reasons = 0;
  int32_t renderer_process_id = 0;
  // Wall time spent streaming the body through the broker.
  base::TimeDelta pump_duration;
  // Thread CPU time spent pumping and hashing the body.
  base::TimeDelta pump_cpu_time;
  ResponseSource response_source = ResponseSource::kNetwork;
};

}  // namespace content

#endif  // CONTENT_SERVICES_RESOURCE_BROKER_OBSERVATION_RECORD_H_
