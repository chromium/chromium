// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/wake_lock/wake_lock_features.h"

namespace features {
// Enables removing wake locks in the WakeLock destructor if non-null. Serves as
// killswitch while we roll out this functionality. TODO(crbug.com/349860794):
// Remove this killswitch post-safe rollout.
BASE_FEATURE(kRemoveWakeLockInDestructor,
             "RemoveWakeLockInDestructor",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features
