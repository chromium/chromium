// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_CPU_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_CPU_PERFORMANCE_H_

#include <cstdint>

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class Navigator;

class CORE_EXPORT NavigatorCPUPerformance final {
 public:
  static uint16_t cpuPerformance(Navigator&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_CPU_PERFORMANCE_H_
