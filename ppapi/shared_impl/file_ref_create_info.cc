// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_ref_create_info.h"

#include <stddef.h>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ppapi/c/pp_file_info.h"

namespace ppapi {

namespace {

std::string GetNameForExternalFilePath(const base::FilePath& in_path) {
  const base::FilePath::StringType& path = in_path.value();
  size_t pos = path.rfind(base::FilePath::kSeparators[0]);
  CHECK(pos != base::FilePath::StringType::npos);
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(path.substr(pos + 1));
#elif BUILDFLAG(IS_POSIX)
  return path.substr(pos + 1);
#else
#error "Unsupported platform."
#endif
}

}  // namespace

bool FileRefCreateInfo::IsValid() const {
  return file_system_type != PP_FILESYSTEMTYPE_INVALID;
}

FileRefCreateInfo MakeExternalFileRefCreateInfo(
    const base::FilePath& external_path,
    const std::string& display_name,
    int browser_pending_host_resource_id,
    int renderer_pending_host_resource_id) {
  FileRefCreateInfo info;
  info.file_system_type = PP_FILESYSTEMTYPE_EXTERNAL;
  if (!display_name.empty())
    info.display_name = display_name;
  else
    info.display_name = GetNameForExternalFilePath(external_path);
  info.browser_pending_host_resource_id = browser_pending_host_resource_id;
  info.renderer_pending_host_resource_id = renderer_pending_host_resource_id;
  return info;
}

}  // namespace ppapi
