// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_REGION_CAPTURE_CROP_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_REGION_CAPTURE_CROP_ID_H_

#include "base/token.h"
#include "base/types/strong_alias.h"

namespace blink {

using RegionCaptureCropId =
    base::StrongAlias<class RegionCaptureCropIdTag, base::Token>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_REGION_CAPTURE_CROP_ID_H_
