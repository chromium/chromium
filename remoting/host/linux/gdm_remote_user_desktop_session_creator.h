// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GDM_REMOTE_USER_DESKTOP_SESSION_CREATOR_H_
#define REMOTING_HOST_LINUX_GDM_REMOTE_USER_DESKTOP_SESSION_CREATOR_H_

#include <string>
#include <string_view>
#include <tuple>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/remote_user_desktop_session_creator.h"

namespace remoting {

// RemoteUserDesktopSessionCreator implementation that calls GDM's
// `RemoteDisplayFactory.CreateUserDisplay` D-Bus method (GDM 50+). GDM starts
// a headless session for the user, which logs in via GDM's `gdm-direct-login`
// PAM service (or `gdm-autologin` on GDM versions without it).
class GdmRemoteUserDesktopSessionCreator final
    : public RemoteUserDesktopSessionCreator {
 public:
  // `connection` must be an initialized system bus connection.
  explicit GdmRemoteUserDesktopSessionCreator(GDBusConnectionRef connection);
  ~GdmRemoteUserDesktopSessionCreator() override;

  GdmRemoteUserDesktopSessionCreator(
      const GdmRemoteUserDesktopSessionCreator&) = delete;
  GdmRemoteUserDesktopSessionCreator& operator=(
      const GdmRemoteUserDesktopSessionCreator&) = delete;

  // RemoteUserDesktopSessionCreator implementation.
  void CreateSession(const std::string& username, Callback callback) override;

 private:
  friend class GdmRemoteUserDesktopSessionCreatorTest;

  // Returns whether `error` from `CreateUserDisplay` means that GDM isn't
  // running or doesn't support `CreateUserDisplay`.
  static bool IsNotSupportedError(std::string_view error);

  void OnCreateUserDisplayResult(std::string username,
                                 Callback callback,
                                 base::expected<std::tuple<>, Loggable> result);

  SEQUENCE_CHECKER(sequence_checker_);

  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<GdmRemoteUserDesktopSessionCreator> weak_ptr_factory_{
      this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GDM_REMOTE_USER_DESKTOP_SESSION_CREATOR_H_
