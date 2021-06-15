// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_H_
#define REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_H_

#include "base/callback.h"
#include "remoting/proto/url_forwarder_control.pb.h"

namespace remoting {

// A helper class to configure the remote URL forwarder (i.e. making it the
// default browser of the OS).
class UrlForwarderConfigurator {
 public:
  using IsUrlForwarderSetUpCallback = base::OnceCallback<void(bool)>;
  using SetUpUrlForwarderCallback = base::RepeatingCallback<void(
      protocol::UrlForwarderControl::SetUpUrlForwarderResponse::State)>;

  static UrlForwarderConfigurator* GetInstance();

  // Runs |callback| with a boolean indicating whether the URL forwarder has
  // been properly set up.
  virtual void IsUrlForwarderSetUp(IsUrlForwarderSetUpCallback callback) = 0;

  // Sets the URL forwarder as the default browser; calls |callback| with any
  // state changes during the setup process. |callback| may be called multiple
  // times until the final state (either COMPLETE or ERROR) is reached.
  virtual void SetUpUrlForwarder(const SetUpUrlForwarderCallback& callback) = 0;

 protected:
  UrlForwarderConfigurator();
  virtual ~UrlForwarderConfigurator();
};

}  // namespace remoting

#endif  // REMOTING_HOST_URL_FORWARDER_CONFIGURATOR_H_
