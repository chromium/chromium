// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/fake_url_forwarder_configurator.h"

namespace remoting {

FakeUrlForwarderConfigurator::FakeUrlForwarderConfigurator() = default;

FakeUrlForwarderConfigurator::~FakeUrlForwarderConfigurator() = default;

void FakeUrlForwarderConfigurator::IsUrlForwarderSetUp(
    IsUrlForwarderSetUpCallback callback) {
  std::move(callback).Run(false);
}

void FakeUrlForwarderConfigurator::SetUpUrlForwarder(
    const SetUpUrlForwarderCallback& callback) {
  callback.Run(
      protocol::UrlForwarderControl::SetUpUrlForwarderResponse::FAILED);
}

}  // namespace remoting
