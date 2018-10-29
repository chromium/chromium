// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_FILE_CHOOSER_FILE_INFO_H_
#define CONTENT_PUBLIC_COMMON_FILE_CHOOSER_FILE_INFO_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Result of file chooser.
//
// This represents either a native file or a non-native file. If
// |file_system_url.is_valid()|, this is a non-native file and |file_path| is
// empty.
struct CONTENT_EXPORT FileChooserFileInfo {
  FileChooserFileInfo();
  FileChooserFileInfo(const FileChooserFileInfo& other);
  ~FileChooserFileInfo();

  // For native files.
  base::FilePath file_path;
  base::FilePath::StringType display_name;

  // For non-native files.
  GURL file_system_url;
  base::Time modification_time;
  int64_t length;
  bool is_directory;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_FILE_CHOOSER_FILE_INFO_H_
