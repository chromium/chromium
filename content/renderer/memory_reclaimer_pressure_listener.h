// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEMORY_RECLAIMER_PRESSURE_LISTENER_H_
#define CONTENT_RENDERER_MEMORY_RECLAIMER_PRESSURE_LISTENER_H_

#include "base/memory/memory_pressure_listener.h"

namespace content {

class MemoryReclaimerPressureListener : public base::MemoryPressureListener {
 public:
  MemoryReclaimerPressureListener();
  ~MemoryReclaimerPressureListener() override;

  void OnMemoryPressure(base::MemoryPressureLevel level) override;

 private:
  base::AsyncMemoryPressureListenerRegistration
      memory_pressure_listener_registration_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEMORY_RECLAIMER_PRESSURE_LISTENER_H_
