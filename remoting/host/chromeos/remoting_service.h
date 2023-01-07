// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_REMOTING_SERVICE_H_
#define REMOTING_HOST_CHROMEOS_REMOTING_SERVICE_H_

#include <memory>

namespace remoting {

class ChromotingHostContext;
class PolicyWatcher;
class RemoteSupportHostAsh;

// The RemotingService is a singleton which provides access to remoting
// functionality to external callers in Chrome OS. This service also manages
// state and lifetime of the instances which implement that functionality.
// This service expects to be called on the sequence it was first called on
// which is bound to the Main/UI sequence in production code.
class RemotingService {
 public:
  static RemotingService& Get();
  virtual ~RemotingService() = default;

  // Must be called on the sequence the service was created on.
  virtual RemoteSupportHostAsh& GetSupportHost() = 0;

  // Can be called on any sequence.
  virtual std::unique_ptr<ChromotingHostContext> CreateHostContext() = 0;

  // Can be called on any sequence.
  virtual std::unique_ptr<PolicyWatcher> CreatePolicyWatcher() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_REMOTING_SERVICE_H_
