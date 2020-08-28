// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_H_

#include <string>
#include "content/common/navigation_params.mojom-forward.h"

namespace content {

CONTENT_EXPORT mojom::CommonNavigationParamsPtr CreateCommonNavigationParams();
CONTENT_EXPORT mojom::CommitNavigationParamsPtr CreateCommitNavigationParams();
CONTENT_EXPORT mojom::InitiatorCSPInfoPtr CreateInitiatorCSPInfo();

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_H_
