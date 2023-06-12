// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/permission_checker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/mac/permission_process_utils.h"
#include "remoting/host/version.h"

namespace remoting::mac {

PermissionChecker::PermissionChecker(
    HostMode mode,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : mode_(mode), io_task_runner_(io_task_runner) {}
PermissionChecker::~PermissionChecker() = default;

std::string PermissionChecker::GetBundleName() {
  std::string host_bundle;
  if (mode_ == HostMode::ME2ME) {
    host_bundle = HOST_BUNDLE_NAME;
  } else {
    host_bundle = REMOTE_ASSISTANCE_HOST_BUNDLE_NAME;
  }

  // Strip off the ".app" suffix.
  auto dot_position = host_bundle.rfind('.');
  if (dot_position != std::string::npos) {
    host_bundle = host_bundle.substr(0, dot_position);
  }
  return host_bundle;
}

void PermissionChecker::CheckAccessibilityPermission(
    PermissionWizard::ResultCallback onResult) {
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(remoting::mac::CheckAccessibilityPermission, mode_),
      std::move(onResult));
}

void PermissionChecker::CheckScreenRecordingPermission(
    PermissionWizard::ResultCallback onResult) {
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(remoting::mac::CheckScreenRecordingPermission, mode_),
      std::move(onResult));
}

}  // namespace remoting::mac
