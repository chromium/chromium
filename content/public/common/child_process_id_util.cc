// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/child_process_id_util.h"

namespace content {

network::OriginatingProcessId ToOriginatingProcessId(ChildProcessId process) {
  // This normalises invalid input to invalid output.  Note that an invalid
  // input could have the values 0 or -1, however an invalid output will always
  // have the value -1.  This disallows the use of the magic value 0 from
  // meaning the browser process.
  if (!process) {
    return network::OriginatingProcessId();
  }
  return network::OriginatingProcessId::renderer(ToRendererProcessId(process));
}

network::RendererProcessId ToRendererProcessId(ChildProcessId process) {
  // This normalises invalid input to invalid output.  Note that an invalid
  // input could have the values 0 or -1, however an invalid output will always
  // have the value -1.  This disallows the use of the magic value 0 from
  // meaning the browser process.
  if (!process) {
    return network::RendererProcessId();
  }
  return network::RendererProcessId(process.value());
}

ChildProcessId ToChildProcessId(network::RendererProcessId process) {
  return ChildProcessId(process.value());
}

network::OriginatingProcessId ToOriginatingProcessIdUnsafe(int32_t process_id) {
  if (process_id == 0) {
    return network::OriginatingProcessId::browser();
  }
  return ToOriginatingProcessId(ChildProcessId::FromUnsafeValue(process_id));
}

}  // namespace content
