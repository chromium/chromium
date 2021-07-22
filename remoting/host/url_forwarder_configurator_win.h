// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_WIN_H_
#define REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_WIN_H_

#include "remoting/host/url_forwarder_configurator.h"

namespace remoting {

// Windows implementation of UrlForwarderConfigurator.
class UrlForwarderConfiguratorWin final : public UrlForwarderConfigurator {
 public:
  UrlForwarderConfiguratorWin();

  void IsUrlForwarderSetUp(IsUrlForwarderSetUpCallback callback) override;
  void SetUpUrlForwarder(const SetUpUrlForwarderCallback& callback) override;

  UrlForwarderConfiguratorWin(const UrlForwarderConfiguratorWin&) = delete;
  UrlForwarderConfiguratorWin& operator=(const UrlForwarderConfiguratorWin&) =
      delete;

 private:
  ~UrlForwarderConfiguratorWin() override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_WIN_H_
