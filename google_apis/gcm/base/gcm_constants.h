// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_BASE_GCM_CONSTANTS_H_
#define GOOGLE_APIS_GCM_BASE_GCM_CONSTANTS_H_

#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"

namespace gcm {

GCM_EXPORT extern const base::TimeDelta kIncomingMessageTTL;

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_BASE_GCM_CONSTANTS_H_
