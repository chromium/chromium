// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FEATURES_H_
#define CONTENT_COMMON_FEATURES_H_

#include "base/compiler_specific.h"
#include "base/feature_list.h"

namespace content {

// Please keep features in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
// Unifies RenderWidgetHostViewAndroid with the other platforms in their usage
// of OnShowWithPageVisibility. Disabling will revert the refactor and use the
// direct ShowInternal path.
extern CONSTINIT base::Feature kOnShowWithPageVisibility;
#endif  // BUILDFLAG(IS_ANDROID)

// Please keep features in alphabetical order.

}  // namespace content

#endif  // CONTENT_COMMON_FEATURES_H_