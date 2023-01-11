// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_REGISTRATION_MANAGER_H_
#define REMOTING_SIGNALING_REGISTRATION_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace remoting {

class ProtobufHttpStatus;

// Interface for registering the user with signaling service.
class RegistrationManager {
 public:
  using DoneCallback = base::OnceCallback<void(const ProtobufHttpStatus&)>;

  virtual ~RegistrationManager() = default;

  // Performs a SignInGaia call for this device. |on_done| is called once it has
  // successfully signed in or failed to sign in.
  virtual void SignInGaia(DoneCallback on_done) = 0;

  // Clears locally cached registration ID and auth token. Caller will need to
  // call SignInGaia() again if they need to get a new auth token.
  virtual void SignOut() = 0;

  virtual bool IsSignedIn() const = 0;

  // Returns empty string if user hasn't been signed in.
  virtual std::string GetRegistrationId() const = 0;
  virtual std::string GetFtlAuthToken() const = 0;

 protected:
  RegistrationManager() = default;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_REGISTRATION_MANAGER_H_
