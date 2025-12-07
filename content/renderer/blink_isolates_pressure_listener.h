// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BLINK_ISOLATES_PRESSURE_LISTENER_H_
#define CONTENT_RENDERER_BLINK_ISOLATES_PRESSURE_LISTENER_H_

#include "base/memory/memory_pressure_listener.h"

namespace content {

class BlinkIsolatesPressureListener : public base::MemoryPressureListener {
 public:
  BlinkIsolatesPressureListener();
  ~BlinkIsolatesPressureListener() override;

  // base::MemoryPressureListener:
  void OnMemoryPressure(base::MemoryPressureLevel level) override;

  void OnRendererVisible();
  void OnRendererHidden();

 private:
  base::AsyncMemoryPressureListenerRegistration
      memory_pressure_listener_registration_;

  bool is_renderer_visible_ = true;
};

}  // namespace content

#endif  // CONTENT_RENDERER_BLINK_ISOLATES_PRESSURE_LISTENER_H_
