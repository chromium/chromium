// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/child_process_id_util.h"

namespace content {

network::OriginatingProcess ToOriginatingProcess(
    const ChildProcessId& process) {
  // This normalises invalid input to invalid output.  Note that an invalid
  // input could have the values 0 or -1, however an invalid output will always
  // have the value -1.  This disallows the use of the magic value 0 from
  // meaning the browser process.
  if (!process) {
    return network::OriginatingProcess();
  }
  return network::OriginatingProcess::renderer(
      network::RendererProcess(process.value()));
}

}  // namespace content
