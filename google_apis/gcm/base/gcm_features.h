// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_BASE_GCM_FEATURES_H_
#define GOOGLE_APIS_GCM_BASE_GCM_FEATURES_H_

#include "base/feature_list.h"
#include "google_apis/gcm/base/gcm_export.h"

namespace gcm::features {

// Kill switch for removing deprecated account mapping data and registrations
// from GCMStore on load.
// TODO(crbug.com/384891711): Remove after October 2027.
GCM_EXPORT BASE_DECLARE_FEATURE(kGCMRemoveDeprecatedStoreData);

}  // namespace gcm::features

#endif  // GOOGLE_APIS_GCM_BASE_GCM_FEATURES_H_
