// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/mojo_util.h"

#include "build/build_config.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"

namespace remoting {

bool IsMojoIpczEnabled() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  // MojoIpcz has only been tested and verified on Linux and Windows.
  // TODO(crbug.com/1378803): Verify that enabling MojoIpcz doesn't break Mac
  // and ChromeOS, then remove helpers in this file.
  return mojo::core::IsMojoIpczEnabled();
#else
  return false;
#endif
}

void InitializeMojo(const mojo::core::Configuration& config) {
  mojo::core::Configuration new_config = config;
  if (!IsMojoIpczEnabled()) {
    new_config.disable_ipcz = true;
    // Disable |is_broker_process|. Legacy mojo core does not require having
    // broker processes in the process graph.
    new_config.is_broker_process = false;
  }
  mojo::core::Init(new_config);
}

}  // namespace remoting
