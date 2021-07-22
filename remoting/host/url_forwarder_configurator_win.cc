// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/url_forwarder_configurator_win.h"

#include "base/no_destructor.h"
#include "base/notreached.h"

namespace remoting {

UrlForwarderConfiguratorWin::UrlForwarderConfiguratorWin() = default;

UrlForwarderConfiguratorWin::~UrlForwarderConfiguratorWin() = default;

void UrlForwarderConfiguratorWin::IsUrlForwarderSetUp(
    IsUrlForwarderSetUpCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(true);
}

void UrlForwarderConfiguratorWin::SetUpUrlForwarder(
    const SetUpUrlForwarderCallback& callback) {
  NOTIMPLEMENTED();
  callback.Run(
      protocol::UrlForwarderControl::SetUpUrlForwarderResponse::COMPLETE);
}

// static
UrlForwarderConfigurator* UrlForwarderConfigurator::GetInstance() {
  static base::NoDestructor<UrlForwarderConfiguratorWin> instance;
  return instance.get();
}

}  // namespace remoting
