// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AGENT_INTEGRATION_CONFIGURATOR_H_
#define REMOTING_HOST_AGENT_INTEGRATION_CONFIGURATOR_H_

#include <memory>

#include "base/functional/callback.h"

namespace remoting {

// A helper class to query and configure coding agent integration on the host.
class AgentIntegrationConfigurator {
 public:
  using IsSupportedCallback = base::OnceCallback<void(bool)>;
  using IsAgentIntegrationEnabledCallback = base::OnceCallback<void(bool)>;
  using SetAgentIntegrationEnabledCallback = base::OnceCallback<void(bool)>;

  static constexpr char kBinaryName[] =
      "chrome-remote-desktop-agent-integration";

  // Runs `callback` with a boolean indicating whether agent integration is
  // supported on the host (i.e. an executable
  // `chrome-remote-desktop-agent-integration` binary exists in the host's
  // executable directory).
  static void IsSupported(IsSupportedCallback callback);

  static std::unique_ptr<AgentIntegrationConfigurator> Create();

  virtual ~AgentIntegrationConfigurator();

  // Runs `callback` with a boolean indicating whether agent integration is
  // currently enabled.
  // NOTE: `callback` may be called after `this` is destroyed. Make sure your
  // callback can handle it.
  virtual void IsAgentIntegrationEnabled(
      IsAgentIntegrationEnabledCallback callback) = 0;

  // Enables or disables agent integration; runs `callback` with a boolean
  // indicating whether the operation succeeded.
  // NOTE: `callback` may be called after `this` is destroyed. Make sure your
  // callback can handle it.
  virtual void SetAgentIntegrationEnabled(
      bool enabled,
      SetAgentIntegrationEnabledCallback callback) = 0;

 protected:
  AgentIntegrationConfigurator();
};

}  // namespace remoting

#endif  // REMOTING_HOST_AGENT_INTEGRATION_CONFIGURATOR_H_
