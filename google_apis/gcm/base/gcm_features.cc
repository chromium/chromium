// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/base/gcm_features.h"

namespace gcm {
namespace features {

BASE_FEATURE(kGCMDeleteIncomingMessagesWithoutTTL,
             "GCMDeleteIncomingMessagesWithoutTTL",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace gcm
