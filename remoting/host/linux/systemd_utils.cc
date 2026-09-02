// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/systemd_utils.h"

#include <systemd/sd-login.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/posix/safe_strerror.h"
#include "remoting/base/logging.h"

namespace remoting {

bool IsRunningInHeadlessSystemdSession() {
  return IsRunningInHeadlessSystemdSession(&sd_pid_get_session,
                                           &sd_session_is_remote);
}

bool IsRunningInHeadlessSystemdSession(
    SdPidGetSessionFunction pid_get_session_func,
    SdSessionIsRemoteFunction session_is_remote_func) {
  DCHECK(pid_get_session_func);
  DCHECK(session_is_remote_func);

  char* raw_session_id = nullptr;
  int ret = pid_get_session_func(0, &raw_session_id);
  if (ret < 0) {
    LOG(ERROR) << "Failed to get systemd session ID: "
               << base::safe_strerror(-ret);
    return false;
  }
  if (!raw_session_id) {
    LOG(ERROR) << "Failed to get systemd session ID: null session ID returned";
    return false;
  }
  std::unique_ptr<char, base::FreeDeleter> session_id(raw_session_id);

  // Login sessions created by GDM's remote display API all have the
  // `Remote=yes` property.
  ret = session_is_remote_func(session_id.get());
  if (ret < 0) {
    LOG(ERROR) << "sd_session_is_remote failed for session " << session_id.get()
               << ": " << base::safe_strerror(-ret);
    return false;
  }

  bool is_remote = (ret > 0);
  HOST_LOG << "Systemd session " << session_id.get()
           << (is_remote ? " is remote (headless)." : " is not remote.");
  // TODO(yuweih): Also check if the PAM service is
  // "chrome-remote-desktop-session" using sd_session_get_service(). CRD is not
  // capable of running login sessions with Remote=yes, so we will need to check
  // the service name instead.
  return is_remote;
}

}  // namespace remoting
