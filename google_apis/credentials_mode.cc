// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/credentials_mode.h"

#include "services/network/public/mojom/fetch_api.mojom.h"

namespace google_apis {

network::mojom::CredentialsMode GetOmitCredentialsModeForGaiaRequests() {
  return network::mojom::CredentialsMode::kOmitBug_775438_Workaround;
}

}  // namespace google_apis
