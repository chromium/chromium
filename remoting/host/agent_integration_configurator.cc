// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/agent_integration_configurator.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "build/build_config.h"

namespace remoting {

AgentIntegrationConfigurator::AgentIntegrationConfigurator() = default;

AgentIntegrationConfigurator::~AgentIntegrationConfigurator() = default;

#if !BUILDFLAG(IS_LINUX)

// static
void AgentIntegrationConfigurator::IsSupported(IsSupportedCallback callback) {
  std::move(callback).Run(false);
}

// static
std::unique_ptr<AgentIntegrationConfigurator>
AgentIntegrationConfigurator::Create() {
  // Unsupported platforms. Use NOTIMPLEMENTED() instead of NOTREACHED() since
  // some unit tests still attempt to create the configurator.
  NOTIMPLEMENTED();
  return nullptr;
}

#endif  // !BUILDFLAG(IS_LINUX)

}  // namespace remoting
