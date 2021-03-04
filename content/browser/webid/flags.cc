// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flags.h"

#include "base/command_line.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

// Whether WebID is enabled or not.
bool IsWebIDEnabled() {
  return base::FeatureList::IsEnabled(features::kWebID);
}

}  // namespace content
