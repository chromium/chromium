// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_type.h"

namespace content {

bool IsResourceTypeFrame(ResourceType type) {
  return type == ResourceType::kMainFrame || type == ResourceType::kSubFrame;
}

}  // namespace content
