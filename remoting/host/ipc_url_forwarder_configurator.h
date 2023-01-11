// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_URL_FORWARDER_CONFIGURATOR_H_
#define REMOTING_HOST_IPC_URL_FORWARDER_CONFIGURATOR_H_

#include "remoting/host/remote_open_url/url_forwarder_configurator.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace remoting {

class DesktopSessionProxy;

// A UrlForwarderConfigurator implementation that delegates calls to a
// DesktopSessionProxy.
class IpcUrlForwarderConfigurator final : public UrlForwarderConfigurator {
 public:
  explicit IpcUrlForwarderConfigurator(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);
  ~IpcUrlForwarderConfigurator() override;

  // UrlForwarderConfigurator implementation.
  void IsUrlForwarderSetUp(IsUrlForwarderSetUpCallback callback) override;
  void SetUpUrlForwarder(const SetUpUrlForwarderCallback& callback) override;

  IpcUrlForwarderConfigurator(const IpcUrlForwarderConfigurator&) = delete;
  IpcUrlForwarderConfigurator& operator=(const IpcUrlForwarderConfigurator&) =
      delete;

 private:
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_URL_FORWARDER_CONFIGURATOR_H_
