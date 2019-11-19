// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_COORDINATOR_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_COORDINATOR_SERVICE_H_

#include "content/common/content_export.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/mojom/resource_coordinator_service.mojom.h"

namespace content {

// Gets the browser's connection to the in-process Resource Coordinator service.
CONTENT_EXPORT resource_coordinator::mojom::ResourceCoordinatorService*
GetResourceCoordinatorService();

// Gets the browser's connection to the Resource Coordinator's
// memory instrumentation CoordinatorController.
CONTENT_EXPORT memory_instrumentation::mojom::CoordinatorController*
GetMemoryInstrumentationCoordinatorController();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_COORDINATOR_SERVICE_H_
