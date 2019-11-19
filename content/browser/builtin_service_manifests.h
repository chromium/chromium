// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BUILTIN_SERVICE_MANIFESTS_H_
#define CONTENT_BROWSER_BUILTIN_SERVICE_MANIFESTS_H_

#include <vector>

#include "services/service_manager/public/cpp/manifest.h"

namespace content {

// Returns the list of manifests for all in-process and out-of-process services
// built into the Content layer.
const std::vector<service_manager::Manifest>& GetBuiltinServiceManifests();

}  // namespace content

#endif  // CONTENT_BROWSER_BUILTIN_SERVICE_MANIFESTS_H_
