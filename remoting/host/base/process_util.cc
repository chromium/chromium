// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/process_util.h"

#include <string.h>

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif

namespace remoting {

base::FilePath GetProcessImagePath(base::ProcessId pid) {
#if BUILDFLAG(IS_LINUX)
  // See: https://man7.org/linux/man-pages/man5/procfs.5.html. It says the
  // symlink will have "(deleted)" appended to the original pathname, but in
  // practice it also has a preceding space.
  static constexpr char kExeDeletedSuffix[] = " (deleted)";
  // We don't get the image path from the command line, since it's spoofable by
  // the process itself.
  base::FilePath process_exe_path(
      base::StringPrintf("/proc/%" CrPRIdPid "/exe", pid));
  base::FilePath process_image_path;
  base::ReadSymbolicLink(process_exe_path, &process_image_path);
  // TODO(yuweih): See if we can run this method on a worker thread that allows
  // blocking and use base::PathExists() instead.
  bool path_exists = access(process_image_path.value().c_str(), F_OK) == 0;
  if (!path_exists) {
    std::string process_base_name = process_image_path.BaseName().value();
    if (!base::EndsWith(process_base_name, kExeDeletedSuffix,
                        base::CompareCase::INSENSITIVE_ASCII)) {
      LOG(ERROR) << "Unexpected process image path: " << process_image_path
                 << ". Path doesn't exist and doesn't end with '"
                 << kExeDeletedSuffix << "'";
      return base::FilePath();
    }
    VLOG(1) << "Process image for PID " << pid
            << " no longer exists in its original path.";
    return process_image_path.DirName().Append(process_base_name.substr(
        0, process_base_name.size() - strlen(kExeDeletedSuffix)));
  }
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
