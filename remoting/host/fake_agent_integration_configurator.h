// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_AGENT_INTEGRATION_CONFIGURATOR_H_
#define REMOTING_HOST_FAKE_AGENT_INTEGRATION_CONFIGURATOR_H_

#include "remoting/host/agent_integration_configurator.h"

namespace remoting {

// A fake AgentIntegrationConfigurator implementation that responds with false.
class FakeAgentIntegrationConfigurator final
    : public AgentIntegrationConfigurator {
 public:
  FakeAgentIntegrationConfigurator();
  ~FakeAgentIntegrationConfigurator() override;

  void IsAgentIntegrationEnabled(
      IsAgentIntegrationEnabledCallback callback) override;
  void SetAgentIntegrationEnabled(
      bool enabled,
      SetAgentIntegrationEnabledCallback callback) override;

  FakeAgentIntegrationConfigurator(const FakeAgentIntegrationConfigurator&) =
      delete;
  FakeAgentIntegrationConfigurator& operator=(
      const FakeAgentIntegrationConfigurator&) = delete;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_AGENT_INTEGRATION_CONFIGURATOR_H_
