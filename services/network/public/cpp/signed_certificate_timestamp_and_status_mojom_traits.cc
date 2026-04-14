// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/signed_certificate_timestamp_and_status_mojom_traits.h"

#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "services/network/public/cpp/sct_status_flags_mojom_traits.h"
#include "services/network/public/cpp/signed_certificate_timestamp_mojom_traits.h"
#include "services/network/public/mojom/signed_certificate_timestamp_and_status.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<network::mojom::SignedCertificateTimestampAndStatusDataView,
                  net::SignedCertificateTimestampAndStatus>::
    Read(network::mojom::SignedCertificateTimestampAndStatusDataView data,
         net::SignedCertificateTimestampAndStatus* out) {
  return data.ReadSct(&out->sct) && data.ReadStatus(&out->status);
}

}  // namespace mojo
