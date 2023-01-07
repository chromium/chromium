// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_SERVICE_H_

#include "content/common/content_export.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"

namespace content {

// Returns an interface to the global instance of the Tracing service. The
// service is started lazily as-needed, so this always returns a valid interface
// reference.
//
// Must be called on the UI thread only.
CONTENT_EXPORT tracing::mojom::TracingService& GetTracingService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_SERVICE_H_
