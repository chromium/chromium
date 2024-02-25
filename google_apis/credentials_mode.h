// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CREDENTIALS_MODE_H_
#define GOOGLE_APIS_CREDENTIALS_MODE_H_

#include "base/component_export.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"

namespace google_apis {

// Returns the CredentialsMode that should be used for uncredentialed requests
// to Gaia.
COMPONENT_EXPORT(GOOGLE_APIS)
network::mojom::CredentialsMode GetOmitCredentialsModeForGaiaRequests();

}  // namespace google_apis

#endif  // GOOGLE_APIS_CREDENTIALS_MODE_H_
