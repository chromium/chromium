// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/mojo_util.h"

#include "build/build_config.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"

namespace remoting {

void InitializeMojo(const mojo::core::Configuration& config) {
  mojo::core::Configuration new_config = config;
#if !BUILDFLAG(IS_LINUX)
  // MojoIpcz has only been tested and verified on Linux. The Windows
  // multi-process architecture doesn't support MojoIpcz yet.
  new_config.disable_ipcz = true;

  // Also, disable |is_broker_process|. Legacy mojo core does not require having
  // broker processes in the process graph. The Windows process graph needs to
  // be fixed to support broker processes, since a broker client cannot connect
  // to a non-broker server.
  new_config.is_broker_process = false;
#endif
  mojo::core::Init(new_config);
}

}  // namespace remoting
