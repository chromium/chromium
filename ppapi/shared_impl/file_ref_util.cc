// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_ref_util.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace ppapi {

std::string GetNameForInternalFilePath(const std::string& path) {
  if (path == "/")
    return path;
  size_t pos = path.rfind('/');
  CHECK(pos != std::string::npos);
  return path.substr(pos + 1);
}

std::string GetNameForExternalFilePath(const base::FilePath& path) {
  const base::FilePath::StringType& file_path = path.value();
  size_t pos = file_path.rfind(base::FilePath::kSeparators[0]);
  CHECK(pos != base::FilePath::StringType::npos);
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(file_path.substr(pos + 1));
#elif BUILDFLAG(IS_POSIX)
  return file_path.substr(pos + 1);
#else
#error "Unsupported platform."
#endif
}

bool IsValidInternalPath(const std::string& path) {
  // We check that:
  //   The path starts with '/'
  //   The path must contain valid UTF-8 characters.
  //   It must not FilePath::ReferencesParent().
  if (path.empty() || !base::IsStringUTF8(path) || path[0] != '/')
    return false;
  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(path);
  if (file_path.ReferencesParent())
    return false;
  return true;
}

bool IsValidExternalPath(const base::FilePath& path) {
  return !path.empty() && !path.ReferencesParent();
}

void NormalizeInternalPath(std::string* path) {
  if (path->size() > 1 && path->back() == '/')
    path->erase(path->size() - 1, 1);
}

}  // namespace ppapi
