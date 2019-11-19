// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_SWITCH_DEPENDENT_FEATURE_OVERRIDES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_SWITCH_DEPENDENT_FEATURE_OVERRIDES_H_

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace content {

// Returns a list of extra switch-dependent feature overrides to be applied
// during FeatureList initialization.
// TODO(chlily): Test more to understand whether this needs to be called for
// child processes, or if it's sufficient to just call this for the browser
// process and have that state propagate to child processes.
CONTENT_EXPORT std::vector<base::FeatureList::FeatureOverrideInfo>
GetSwitchDependentFeatureOverrides(const base::CommandLine& command_line);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_SWITCH_DEPENDENT_FEATURE_OVERRIDES_H_
