// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/cpu_utils.h"

#include "base/cpu.h"
#include "build/build_config.h"

namespace remoting {

bool IsCpuSupported() {
#if defined(ARCH_CPU_X86_FAMILY)
  // x86 Chromium builds target SSE3.
  // See crbug.com/1251642 for more info.
  if (!base::CPU().has_sse3()) {
    return false;
  }
#endif
  return true;
}

}  // namespace remoting
