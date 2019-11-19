// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_memory_pressure_listener.h"

#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"

namespace blink {

void WebMemoryPressureListener::OnMemoryPressure(
    WebMemoryPressureLevel pressure_level) {
  MemoryPressureListenerRegistry::Instance().OnMemoryPressure(pressure_level);
}

void WebMemoryPressureListener::OnPurgeMemory() {
  MemoryPressureListenerRegistry::Instance().OnPurgeMemory();
}

}  // namespace blink
