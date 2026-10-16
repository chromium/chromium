// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_agent_integration_configurator.h"

#include <utility>

namespace remoting {

FakeAgentIntegrationConfigurator::FakeAgentIntegrationConfigurator() = default;

FakeAgentIntegrationConfigurator::~FakeAgentIntegrationConfigurator() = default;

void FakeAgentIntegrationConfigurator::IsAgentIntegrationEnabled(
    IsAgentIntegrationEnabledCallback callback) {
  std::move(callback).Run(false);
}

void FakeAgentIntegrationConfigurator::SetAgentIntegrationEnabled(
    bool enabled,
    SetAgentIntegrationEnabledCallback callback) {
  std::move(callback).Run(false);
}

}  // namespace remoting
