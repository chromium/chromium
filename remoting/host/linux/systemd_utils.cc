// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/systemd_utils.h"

#include <systemd/sd-login.h>

#include <memory>
#include <string_view>

#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/posix/safe_strerror.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

// PAM service used by CRD to create login sessions. See
// chrome-remote-desktop-x11-session@.service.
constexpr std::string_view kCrdPamServiceName = "chrome-remote-desktop";

}  // namespace

bool IsRunningInHeadlessSystemdSession() {
  return IsRunningInHeadlessSystemdSession(
      &sd_pid_get_session, &sd_session_is_remote, &sd_session_get_service);
}

bool IsRunningInHeadlessSystemdSession(
    SdPidGetSessionFunction pid_get_session_func,
    SdSessionIsRemoteFunction session_is_remote_func,
    SdSessionGetServiceFunction session_get_service_func) {
  DCHECK(pid_get_session_func);
  DCHECK(session_is_remote_func);
  DCHECK(session_get_service_func);

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

  if (ret > 0) {
    HOST_LOG << "Systemd session " << session_id.get()
             << " is remote (headless).";
    return true;
  }

  // CRD is not capable of creating login sessions with `Remote=yes`, so
  // sessions created by CRD are identified by their PAM service instead.
  char* raw_service = nullptr;
  ret = session_get_service_func(session_id.get(), &raw_service);
  if (ret < 0) {
    LOG(ERROR) << "sd_session_get_service failed for session "
               << session_id.get() << ": " << base::safe_strerror(-ret);
    return false;
  }
  if (!raw_service) {
    LOG(ERROR) << "sd_session_get_service returned null service for session "
               << session_id.get();
    return false;
  }
  std::unique_ptr<char, base::FreeDeleter> service(raw_service);

  bool is_crd_session = std::string_view(service.get()) == kCrdPamServiceName;
  HOST_LOG << "Systemd session " << session_id.get() << " has PAM service "
           << service.get()
           << (is_crd_session ? " (headless)." : " (not headless).");
  return is_crd_session;
}

}  // namespace remoting
