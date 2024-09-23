// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

#include <stdint.h>

#include <cstdio>
#include <sstream>

#include "base/files/file_util.h"
#include "base/format_macros.h"

namespace extensions {

namespace {

const char kProcStat[] = "/proc/stat";

}  // namespace

bool CpuInfoProvider::QueryCpuTimePerProcessor(
    std::vector<api::system_cpu::ProcessorInfo>* infos) {
  DCHECK(infos);

  // WARNING: this method may return incomplete data because some processors may
  // be brought offline at runtime. /proc/stat does not report statistics of
  // offline processors. CPU usages of offline processors will be filled with
  // zeros.
  //
  // An example of output of /proc/stat when processor 0 and 3 are online, but
  // processor 1 and 2 are offline:
  //
  //   cpu  145292 20018 83444 1485410 995 44 3578 0 0 0
  //   cpu0 138060 19947 78350 1479514 570 44 3576 0 0 0
  //   cpu3 2033 32 1075 1400 52 0 1 0 0 0
  std::string contents;
  if (!base::ReadFileToString(base::FilePath(kProcStat), &contents)) {
    return false;
  }

  std::istringstream iss(contents);
  std::string line;

  // Skip the first line because it is just an aggregated number of
  // all cpuN lines.
  std::getline(iss, line);
  while (std::getline(iss, line)) {
    if (line.compare(0, 3, "cpu") != 0) {
      continue;
    }

    uint64_t user = 0, nice = 0, sys = 0, idle = 0;
    uint32_t pindex = 0;
    int vals =
        sscanf(line.c_str(),
               "cpu%" PRIu32 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64,
               &pindex, &user, &nice, &sys, &idle);
    if (vals != 5 || pindex >= infos->size()) {
      // TODO(b/326303922): This fires in internal integration tests, reevaluate
      // whether this should be (and can be made) unreachable or handle it.
      DUMP_WILL_BE_NOTREACHED();
      return false;
    }

    infos->at(pindex).usage.kernel = static_cast<double>(sys);
    infos->at(pindex).usage.user = static_cast<double>(user + nice);
    infos->at(pindex).usage.idle = static_cast<double>(idle);
    infos->at(pindex).usage.total =
        static_cast<double>(sys + user + nice + idle);
  }

  return true;
}

}  // namespace extensions
