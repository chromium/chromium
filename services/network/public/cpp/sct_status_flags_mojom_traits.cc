// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sct_status_flags_mojom_traits.h"

#include "base/notreached.h"
#include "net/cert/sct_status_flags.h"
#include "services/network/public/mojom/sct_status_flags.mojom-shared.h"

namespace mojo {

// static
network::mojom::SCTVerifyStatus
EnumTraits<network::mojom::SCTVerifyStatus, net::ct::SCTVerifyStatus>::ToMojom(
    net::ct::SCTVerifyStatus status) {
  switch (status) {
    case net::ct::SCT_STATUS_LOG_UNKNOWN:
      return network::mojom::SCTVerifyStatus::kLogUnknown;
    case net::ct::SCT_STATUS_INVALID_SIGNATURE:
      return network::mojom::SCTVerifyStatus::kInvalidSignature;
    case net::ct::SCT_STATUS_OK:
      return network::mojom::SCTVerifyStatus::kOk;
    case net::ct::SCT_STATUS_INVALID_TIMESTAMP:
      return network::mojom::SCTVerifyStatus::kInvalidTimestamp;
    case net::ct::SCT_STATUS_NONE:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
net::ct::SCTVerifyStatus
EnumTraits<network::mojom::SCTVerifyStatus, net::ct::SCTVerifyStatus>::
    FromMojom(network::mojom::SCTVerifyStatus input) {
  switch (input) {
    case network::mojom::SCTVerifyStatus::kLogUnknown:
      return net::ct::SCT_STATUS_LOG_UNKNOWN;
    case network::mojom::SCTVerifyStatus::kInvalidSignature:
      return net::ct::SCT_STATUS_INVALID_SIGNATURE;
    case network::mojom::SCTVerifyStatus::kOk:
      return net::ct::SCT_STATUS_OK;
    case network::mojom::SCTVerifyStatus::kInvalidTimestamp:
      return net::ct::SCT_STATUS_INVALID_TIMESTAMP;
  }
  NOTREACHED();
}

}  // namespace mojo
