// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_DIR_CONTENTS_H_
#define PPAPI_SHARED_IMPL_DIR_CONTENTS_H_

#include <vector>

#include "base/files/file_path.h"

namespace ppapi {

struct DirEntry {
  base::FilePath name;
  bool is_dir;
};

typedef std::vector<DirEntry> DirContents;

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_DIR_CONTENTS_H_
