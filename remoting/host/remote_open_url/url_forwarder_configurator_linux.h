// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_LINUX_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_LINUX_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/remote_open_url/url_forwarder_configurator.h"

namespace remoting {

// Linux implementation of UrlForwarderConfigurator.
class UrlForwarderConfiguratorLinux final : public UrlForwarderConfigurator {
 public:
  UrlForwarderConfiguratorLinux();
  ~UrlForwarderConfiguratorLinux() override;

  void IsUrlForwarderSetUp(IsUrlForwarderSetUpCallback callback) override;
  void SetUpUrlForwarder(const SetUpUrlForwarderCallback& callback) override;

  UrlForwarderConfiguratorLinux(const UrlForwarderConfiguratorLinux&) = delete;
  UrlForwarderConfiguratorLinux& operator=(
      const UrlForwarderConfiguratorLinux&) = delete;

 private:
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_LINUX_H_
