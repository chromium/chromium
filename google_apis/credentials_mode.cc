// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/credentials_mode.h"

#include "base/feature_list.h"
#include "services/network/public/mojom/fetch_api.mojom.h"

namespace google_apis {

namespace {

BASE_FEATURE(kGaiaCredentialsModeOmitBug_775438_Workaround,
             "GaiaCredentialsModeOmitBug_775438_Workaround",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

network::mojom::CredentialsMode GetOmitCredentialsModeForGaiaRequests() {
  return base::FeatureList::IsEnabled(
             kGaiaCredentialsModeOmitBug_775438_Workaround)
             ? network::mojom::CredentialsMode::kOmitBug_775438_Workaround
             : network::mojom::CredentialsMode::kOmit;
}

}  // namespace google_apis
