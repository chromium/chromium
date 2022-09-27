// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_NAVIGATION_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_NAVIGATION_PARAMS_H_

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"

namespace blink {

BLINK_COMMON_EXPORT mojom::CommonNavigationParamsPtr
CreateCommonNavigationParams();
BLINK_COMMON_EXPORT mojom::CommitNavigationParamsPtr
CreateCommitNavigationParams();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NAVIGATION_NAVIGATION_PARAMS_H_
