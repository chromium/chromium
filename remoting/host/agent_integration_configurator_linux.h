// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AGENT_INTEGRATION_CONFIGURATOR_LINUX_H_
#define REMOTING_HOST_AGENT_INTEGRATION_CONFIGURATOR_LINUX_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "remoting/host/agent_integration_configurator.h"

namespace remoting {

// Linux implementation of AgentIntegrationConfigurator.
class AgentIntegrationConfiguratorLinux final
    : public AgentIntegrationConfigurator {
 public:
  static constexpr base::TimeDelta kDefaultCommandTimeout = base::Seconds(10);

  AgentIntegrationConfiguratorLinux();
  ~AgentIntegrationConfiguratorLinux() override;

  void IsAgentIntegrationEnabled(
      IsAgentIntegrationEnabledCallback callback) override;
  void SetAgentIntegrationEnabled(
      bool enabled,
      SetAgentIntegrationEnabledCallback callback) override;

  void SetCommandTimeoutForTesting(base::TimeDelta timeout);

  AgentIntegrationConfiguratorLinux(const AgentIntegrationConfiguratorLinux&) =
      delete;
  AgentIntegrationConfiguratorLinux& operator=(
      const AgentIntegrationConfiguratorLinux&) = delete;

 private:
  base::TimeDelta command_timeout_ = kDefaultCommandTimeout;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_AGENT_INTEGRATION_CONFIGURATOR_LINUX_H_
