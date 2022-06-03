// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_USE_ZOOM_FOR_DSF_POLICY_H_
#define CONTENT_PUBLIC_COMMON_USE_ZOOM_FOR_DSF_POLICY_H_

#include "content/common/content_export.h"

// A centralized file for base helper methods and policy decisions about use
// zoom for DSF (i.e., Device Scale Factor).
//
// In the renderer, the decision to UseZoomForDSF should come from the
// RenderThread, not from this global method, so that it can be
// controlled and injected in tests.

namespace content {

CONTENT_EXPORT bool IsUseZoomForDSFEnabled();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_USE_ZOOM_FOR_DSF_POLICY_H_
