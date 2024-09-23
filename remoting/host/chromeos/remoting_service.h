// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_REMOTING_SERVICE_H_
#define REMOTING_HOST_CHROMEOS_REMOTING_SERVICE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "remoting/host/chromeos/session_id.h"

namespace base {
class FilePath;
}  // namespace base

namespace remoting {

class RemoteSupportHostAsh;

// The RemotingService is a singleton which provides access to remoting
// functionality to external callers in Chrome OS. This service also manages
// state and lifetime of the instances which implement that functionality.
// This service expects to be called on the sequence it was first called on
// which is bound to the Main/UI sequence in production code.
class RemotingService {
 public:
  using SessionIdCallback = base::OnceCallback<void(std::optional<SessionId>)>;

  static RemotingService& Get();
  virtual ~RemotingService() = default;

  // Must be called on the sequence the service was created on.
  virtual RemoteSupportHostAsh& GetSupportHost() = 0;

  // Allows the caller to query if information about a reconnectable session is
  // stored. Invokes `callback` with the id of this session (or std::nullopt if
  // there is no reconnectable session).
  virtual void GetReconnectableEnterpriseSessionId(
      SessionIdCallback callback) = 0;

  static void SetSessionStorageDirectoryForTesting(const base::FilePath& dir);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_REMOTING_SERVICE_H_
