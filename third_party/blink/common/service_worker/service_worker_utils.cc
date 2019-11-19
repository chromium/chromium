// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

bool ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled() {
  return base::FeatureList::IsEnabled(
      blink::features::kServiceWorkerImportedScriptUpdateCheck);
}

}  // namespace blink
