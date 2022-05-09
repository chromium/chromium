// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/process_util.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif

namespace remoting {

base::FilePath GetProcessImagePath(base::ProcessId pid) {
#if BUILDFLAG(IS_LINUX)
  // We don't get the image path from the command line, since it's spoofable by
  // the process itself.
  base::FilePath process_exe_path(
      base::StringPrintf("/proc/%" CrPRIdPid "/exe", pid));
  base::FilePath process_image_path;
  base::ReadSymbolicLink(process_exe_path, &process_image_path);
  return process_image_path;
#elif BUILDFLAG(IS_WIN)
  base::win::ScopedHandle process_handle(
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid));
  if (!process_handle.is_valid()) {
    PLOG(ERROR) << "OpenProcess failed";
    return base::FilePath();
  }
  std::array<wchar_t, MAX_PATH + 1> buffer;
  DWORD size = buffer.size();
  if (!QueryFullProcessImageName(process_handle.Get(), 0, buffer.data(),
                                 &size)) {
    PLOG(ERROR) << "QueryFullProcessImageName failed";
    return base::FilePath();
  }
  DCHECK_GT(size, 0u);
  DCHECK_LT(size, buffer.size());
  return base::FilePath(base::FilePath::StringPieceType(buffer.data(), size));
#else
  NOTIMPLEMENTED();
  return base::FilePath();
#endif
}

}  // namespace remoting
