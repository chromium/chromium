// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_features.h"

#include "base/feature_list.h"

namespace gaia::features {

// Enables binary format parsing in the /ListAccounts Gaia call. The endpoint
// response depends on the presence of laf=b64bin parameter in the called url.
BASE_FEATURE(kListAccountsUsesBinaryFormat,
             "ListAccountsUsesBinaryFormat",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Switches account capabilities fetch to the "getAllVisible" URL that fetches
// all capabilities visible to Chrome instead of asking for a hardcoded list of
// capabilities that might be only partially available.
COMPONENT_EXPORT(GOOGLE_APIS)
BASE_FEATURE(kGetAccountCapabilitiesUsesGetAllVisibleUrl,
             "GetAccountCapabilitiesUsesGetAllVisibleUrl",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace gaia::features
