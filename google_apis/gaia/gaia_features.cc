// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_features.h"

namespace gaia::features {

// Enables binary format parsing in the /ListAccounts Gaia call. The endpoint
// response depends on the presence of laf=b64bin parameter in the called url.
BASE_FEATURE(kListAccountsUsesBinaryFormat,
             "ListAccountsUsesBinaryFormat",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace gaia::features
