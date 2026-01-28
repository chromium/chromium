// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/child_process_id_util.h"

namespace content {

network::OriginatingProcess ToOriginatingProcess(ChildProcessId process) {
  // This normalises invalid input to invalid output.  Note that an invalid
  // input could have the values 0 or -1, however an invalid output will always
  // have the value -1.  This disallows the use of the magic value 0 from
  // meaning the browser process.
  if (!process) {
    return network::OriginatingProcess();
  }
  return network::OriginatingProcess::renderer(ToRendererProcess(process));
}

network::RendererProcess ToRendererProcess(ChildProcessId process) {
  // This normalises invalid input to invalid output.  Note that an invalid
  // input could have the values 0 or -1, however an invalid output will always
  // have the value -1.  This disallows the use of the magic value 0 from
  // meaning the browser process.
  if (!process) {
    return network::RendererProcess();
  }
  return network::RendererProcess(process.value());
}

ChildProcessId ToChildProcessId(network::RendererProcess process) {
  return ChildProcessId(process.value());
}

network::OriginatingProcess ToOriginatingProcessUnsafe(int32_t process_id) {
  if (process_id == 0) {
    return network::OriginatingProcess::browser();
  }
  return ToOriginatingProcess(ChildProcessId::FromUnsafeValue(process_id));
}

}  // namespace content
