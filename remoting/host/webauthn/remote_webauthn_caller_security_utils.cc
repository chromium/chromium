// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_caller_security_utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/process/process_handle.h"
#include "remoting/host/base/process_util.h"
#else
#include "base/notreached.h"
#endif

namespace remoting {
namespace {

#if defined(NDEBUG) && BUILDFLAG(IS_LINUX)
constexpr auto kAllowedCallerPrograms =
    base::MakeFixedFlatSet<base::FilePath::StringPieceType>({
        "/opt/google/chrome/chrome",
        "/opt/google/chrome-beta/chrome",
        "/opt/google/chrome-unstable/chrome",
    });
#endif

}  // namespace

bool IsLaunchedByTrustedProcess() {
#if !defined(NDEBUG)
  // Just return true on debug builds for the convenience of development.
  return true;
#elif BUILDFLAG(IS_LINUX)
  base::ProcessId parent_pid =
      base::GetParentProcessId(base::GetCurrentProcessHandle());
  base::FilePath parent_image_path = GetProcessImagePath(parent_pid);
  return kAllowedCallerPrograms.contains(parent_image_path.value());
#else
  // TODO(yuweih): Implement this for Windows.
  NOTIMPLEMENTED();
  return true;
#endif
}

}  // namespace remoting
