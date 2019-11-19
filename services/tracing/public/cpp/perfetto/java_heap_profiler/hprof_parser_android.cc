// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hprof_parser_android.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>

#include "base/android/java_heap_dump_generator.h"
#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_buffer_android.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

namespace tracing {

HprofParser::HprofParser(const std::string& fp) : file_path_(fp) {}

HprofParser::ParseStats::ParseStats() {}

void HprofParser::ParseFileData(const unsigned char* file_data,
                                size_t file_size) {
  HprofBuffer hprof(file_data, file_size);

  uint32_t id_size;
  // Skip all leading 0s until we find the |id_size|.
  while (hprof.GetOneByte() != 0 && hprof.HasRemaining()) {
  }
  id_size = hprof.GetFourBytes();
  hprof.set_id_size(id_size);

  hprof.Skip(4);  // hightime
  hprof.Skip(4);  // lowtime
  while (hprof.HasRemaining()) {
    uint32_t tag = hprof.GetOneByte();
    hprof.Skip(4);  // time
    uint32_t record_length = hprof.GetFourBytes();

    switch (tag) {
      default:
        // Ignore any other tags that we either don't know about or don't
        // care about. For now, Skip everything.
        // TODO(zhanggeorge): Add specific parsing per tag.
        hprof.Skip(record_length);
        break;
    }
  }
  parse_stats_.result = HprofParser::ParseResult::PARSE_SUCCESS;
}

HprofParser::ParseResult HprofParser::Parse() {
  base::ScopedFD fd(open(file_path_.c_str(), O_RDONLY));
  if (!fd.is_valid()) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  struct stat file_stats;
  if (stat(file_path_.c_str(), &file_stats) < 0) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  void* file_data =
      mmap(0, file_stats.st_size, PROT_READ, MAP_PRIVATE, fd.get(), 0);
  if (file_data == MAP_FAILED) {
    parse_stats_.result = HprofParser::ParseResult::FAILED_TO_OPEN_FILE;
    return parse_stats_.result;
  }

  ParseFileData(reinterpret_cast<const unsigned char*>(file_data),
                file_stats.st_size);

  int res = munmap(file_data, file_stats.st_size);
  DCHECK_EQ(res, 0);

  return parse_stats_.result;
}
}  // namespace tracing
