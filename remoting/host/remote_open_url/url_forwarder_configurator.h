// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "remoting/proto/url_forwarder_control.pb.h"

namespace remoting {

// A helper class to configure the remote URL forwarder (i.e. making it the
// default browser of the OS).
class UrlForwarderConfigurator {
 public:
  using SetUpUrlForwarderResponse =
      protocol::UrlForwarderControl::SetUpUrlForwarderResponse;
  using IsUrlForwarderSetUpCallback = base::OnceCallback<void(bool)>;
  using SetUpUrlForwarderCallback =
      base::RepeatingCallback<void(SetUpUrlForwarderResponse::State)>;

  static std::unique_ptr<UrlForwarderConfigurator> Create();

  virtual ~UrlForwarderConfigurator();

  // Runs |callback| with a boolean indicating whether the URL forwarder has
  // been properly set up.
  // NOTE: |callback| may be called after |this| is destroyed. Make sure your
  // callback can handle it.
  virtual void IsUrlForwarderSetUp(IsUrlForwarderSetUpCallback callback) = 0;

  // Sets the URL forwarder as the default browser; calls |callback| with any
  // state changes during the setup process. |callback| may be called multiple
  // times until the final state (either COMPLETE or ERROR) is reached.
  // NOTE: |callback| may be called after |this| is destroyed. Make sure your
  // callback can handle it.
  virtual void SetUpUrlForwarder(const SetUpUrlForwarderCallback& callback) = 0;

 protected:
  UrlForwarderConfigurator();
};

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_URL_FORWARDER_CONFIGURATOR_H_
