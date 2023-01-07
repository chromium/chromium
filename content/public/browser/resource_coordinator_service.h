// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_COORDINATOR_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_COORDINATOR_SERVICE_H_

#include "content/common/content_export.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/registry.h"

namespace content {

CONTENT_EXPORT memory_instrumentation::Registry*
GetMemoryInstrumentationRegistry();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_COORDINATOR_SERVICE_H_
