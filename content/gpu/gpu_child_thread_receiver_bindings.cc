// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exposes services from the GPU process to the browser.

#include "content/gpu/gpu_child_thread.h"

#include "base/no_destructor.h"
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"
#include "services/shape_detection/shape_detection_service.h"

namespace content {

void GpuChildThread::BindServiceInterface(
    mojo::GenericPendingReceiver receiver) {
  if (auto shape_detection_receiver =
          receiver.As<shape_detection::mojom::ShapeDetectionService>()) {
    static base::NoDestructor<shape_detection::ShapeDetectionService> service{
        std::move(shape_detection_receiver)};
    return;
  }
}

}  // namespace content
