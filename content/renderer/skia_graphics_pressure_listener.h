// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SKIA_GRAPHICS_PRESSURE_LISTENER_H_
#define CONTENT_RENDERER_SKIA_GRAPHICS_PRESSURE_LISTENER_H_

#include "base/memory_coordinator/async_memory_consumer_registration.h"
#include "base/memory_coordinator/memory_consumer.h"

namespace content {

class SkiaGraphicsPressureListener : public base::MemoryConsumer {
 public:
  SkiaGraphicsPressureListener();
  ~SkiaGraphicsPressureListener() override;

  // base::MemoryConsumer:
  void OnUpdateMemoryLimit() override;
  void OnReleaseMemory() override;

 private:
  base::AsyncMemoryConsumerRegistration memory_consumer_registration_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SKIA_GRAPHICS_PRESSURE_LISTENER_H_
