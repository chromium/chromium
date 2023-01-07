// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_SERVICES_PROVIDER_H_
#define REMOTING_HOST_CHROMOTING_HOST_SERVICES_PROVIDER_H_

namespace remoting {

namespace mojom {
class ChromotingSessionServices;
}  // namespace mojom

// Interface that provides ChromotingHostServices APIs.
class ChromotingHostServicesProvider {
 public:
  virtual ~ChromotingHostServicesProvider() = default;

  // Gets the ChromotingHostServices. Returns nullptr if the interface cannot be
  // provided at the moment.
  virtual mojom::ChromotingSessionServices* GetSessionServices() const = 0;

 protected:
  ChromotingHostServicesProvider() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_SERVICES_PROVIDER_H_
