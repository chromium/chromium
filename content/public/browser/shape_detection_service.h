// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SHAPE_DETECTION_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_SHAPE_DETECTION_SERVICE_H_

#include "content/common/content_export.h"
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"

namespace content {

// Returns the browser's remote interface to the global ShapeDetectionService
// instance, which is started lazily.
CONTENT_EXPORT shape_detection::mojom::ShapeDetectionService*
GetShapeDetectionService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SHAPE_DETECTION_SERVICE_H_
