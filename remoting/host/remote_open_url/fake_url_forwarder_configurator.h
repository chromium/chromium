// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_FAKE_URL_FORWARDER_CONFIGURATOR_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_FAKE_URL_FORWARDER_CONFIGURATOR_H_

#include "remoting/host/remote_open_url/url_forwarder_configurator.h"

namespace remoting {

// A fake UrlForwarderConfigurator implementation that responds with either
// false or FAILED.
class FakeUrlForwarderConfigurator final : public UrlForwarderConfigurator {
 public:
  FakeUrlForwarderConfigurator();
  ~FakeUrlForwarderConfigurator() override;

  void IsUrlForwarderSetUp(IsUrlForwarderSetUpCallback callback) override;
  void SetUpUrlForwarder(const SetUpUrlForwarderCallback& callback) override;

  FakeUrlForwarderConfigurator(const FakeUrlForwarderConfigurator&) = delete;
  FakeUrlForwarderConfigurator& operator=(const FakeUrlForwarderConfigurator&) =
      delete;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_FAKE_URL_FORWARDER_CONFIGURATOR_H_
