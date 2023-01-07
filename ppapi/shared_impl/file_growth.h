// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FILE_GROWTH_H_
#define PPAPI_SHARED_IMPL_FILE_GROWTH_H_

#include <map>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

struct PPAPI_SHARED_EXPORT FileGrowth {
  FileGrowth();
  FileGrowth(int64_t max_written_offset, int64_t append_mode_write_amount);

  int64_t max_written_offset;
  int64_t append_mode_write_amount;
};

typedef std::map<int32_t, FileGrowth> FileGrowthMap;
typedef std::map<int32_t, int64_t> FileSizeMap;

PPAPI_SHARED_EXPORT FileGrowthMap
    FileSizeMapToFileGrowthMapForTesting(const FileSizeMap& file_sizes);
PPAPI_SHARED_EXPORT FileSizeMap
    FileGrowthMapToFileSizeMapForTesting(const FileGrowthMap& file_growths);

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FILE_GROWTH_H_
