// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_memory_pressure_listener.h"

#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"

namespace blink {

void WebMemoryPressureListener::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel pressure_level) {
  MemoryPressureListenerRegistry::Instance().OnMemoryPressure(pressure_level);
}

void WebMemoryPressureListener::OnPurgeMemory() {
  MemoryPressureListenerRegistry::Instance().OnPurgeMemory();
}

}  // namespace blink
