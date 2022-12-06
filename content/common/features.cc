// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/features.h"

namespace content {

// Please keep features in alphabetical order.

BASE_FEATURE(kAvoidUnnecessaryNavigationCancellations,
             "AvoidUnnecessaryNavigationCancellations",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOnShowWithPageVisibility,
             "OnShowWithPageVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kQueueNavigationsWhileWaitingForCommit,
             "QueueNavigationsWhileWaitingForPendingCommit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSpeculativeServiceWorkerStartup,
             "SpeculativeServiceWorkerStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Please keep features in alphabetical order.

}  // namespace content
