// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_growth.h"

#include "base/check_op.h"

namespace ppapi {

FileGrowth::FileGrowth() : max_written_offset(0), append_mode_write_amount(0) {}

FileGrowth::FileGrowth(int64_t max_written_offset,
                       int64_t append_mode_write_amount)
    : max_written_offset(max_written_offset),
      append_mode_write_amount(append_mode_write_amount) {
  DCHECK_LE(0, max_written_offset);
  DCHECK_LE(0, append_mode_write_amount);
}

FileGrowthMap FileSizeMapToFileGrowthMapForTesting(
    const FileSizeMap& file_sizes) {
  FileGrowthMap file_growths;
  for (FileSizeMap::const_iterator it = file_sizes.begin();
       it != file_sizes.end();
       ++it)
    file_growths[it->first] = FileGrowth(it->second, 0);
  return file_growths;
}

FileSizeMap FileGrowthMapToFileSizeMapForTesting(
    const FileGrowthMap& file_growths) {
  FileSizeMap file_sizes;
  for (FileGrowthMap::const_iterator it = file_growths.begin();
       it != file_growths.end();
       ++it)
    file_sizes[it->first] = it->second.max_written_offset;
  return file_sizes;
}

}  // namespace ppapi
