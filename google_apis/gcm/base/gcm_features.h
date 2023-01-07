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

}  // namespace features
}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_GCM_FEATURES_H_
