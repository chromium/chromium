// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_BASE_GCM_FEATURES_H_
#define GOOGLE_APIS_GCM_BASE_GCM_FEATURES_H_

#include "base/feature_list.h"
#include "google_apis/gcm/base/gcm_export.h"

namespace gcm {
namespace features {

GCM_EXPORT BASE_DECLARE_FEATURE(kGCMDeleteIncomingMessagesWithoutTTL);

// When enabled, the connection to the server won't be initiated when offline.
GCM_EXPORT BASE_DECLARE_FEATURE(kGCMAvoidConnectionWhenNetworkUnavailable);

// When enabled, the first connection attempt won't contribute to backoff
// exponential delay. This will mitigate issues when there is no network
// connection yet but a lot of network changes.
GCM_EXPORT BASE_DECLARE_FEATURE(kGCMDoNotIncreaseBackoffDelayOnNetworkChange);

}  // namespace features
}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_GCM_FEATURES_H_
