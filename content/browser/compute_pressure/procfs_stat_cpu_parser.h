// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PROCFS_STAT_CPU_PARSER_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PROCFS_STAT_CPU_PARSER_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"

namespace content {

// Parses CPU time usage stats from procfs (/proc/stat).
//
// This class is not thread-safe. Each instance must be used on the same
// sequence, which must allow blocking I/O. The constructor may be used on a
// different sequence.
class CONTENT_EXPORT ProcfsStatCpuParser {
 public:
  // CPU core utilization statistics.
  //
  // Quantities are expressed in "user hertz", which is a Linux kernel
  // configuration knob (USER_HZ). Typical values range between 1/100 seconds
  // and 1/1000 seconds. The denominator can be obtained from
  // sysconf(_SC_CLK_TCK).
  struct CoreTimes {
    // Normal processes executing in user mode.
    uint64_t user() const { return times[0]; }
    // Niced processes executing in user mode.
    uint64_t nice() const { return times[1]; }
    // Processes executing in kernel mode.
    uint64_t system() const { return times[2]; }
    // Twiddling thumbs.
    uint64_t idle() const { return times[3]; }
    // Waiting for I/O to complete. Unreliable.
    uint64_t iowait() const { return times[4]; }
    // Servicing interrupts.
    uint64_t irq() const { return times[5]; }
    // Servicing softirqs.
    uint64_t softirq() const { return times[6]; }
    // Involuntary wait.
    uint64_t steal() const { return times[7]; }
    // Running a normal guest.
    uint64_t guest() const { return times[8]; }
    // Running a niced guest.
    uint64_t guest_nice() const { return times[9]; }

    uint64_t times[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Computes a CPU's utilization over the time between two stat snapshots.
    //
    // Returns a value between 0.0 and 1.0 on success, and -1.0 when given
    // invalid data, such as a `baseline` that does not represent a stat
    // snapshot collected before `this` snapshot.
    double TimeUtilization(const CoreTimes& baseline) const;
  };

  // The "production" `procfs_path` value for the constructor.
  static constexpr base::FilePath::CharType kProcfsStatPath[] =
      FILE_PATH_LITERAL("/proc/stat");

  // `stat_path` is exposed for testing. Production instances should be
  // constructed using base::FilePath(kProcfsStatPath).
  explicit ProcfsStatCpuParser(base::FilePath stat_path);
  ~ProcfsStatCpuParser();

  ProcfsStatCpuParser(const ProcfsStatCpuParser&) = delete;
  ProcfsStatCpuParser& operator=(const ProcfsStatCpuParser&) = delete;

  const std::vector<CoreTimes>& core_times() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return core_times_;
  }

  // Reads /proc/stat and updates the vector returned by core_times().
  //
  // Returns false if parsing fails. This only happens if /proc/stat is not
  // accessible. Invalid data is ignored.
  //
  // Cores without entries in /proc/stat will remain unchanged. Counters that
  // decrease below the last parsed value are ignored.
  bool Update();

 private:
  // Returns -1 if the line does not include any CPU.
  static int CoreIdFromLine(base::StringPiece stat_line);

  static void UpdateCore(base::StringPiece core_line, CoreTimes& core_times);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath stat_path_;

  std::vector<CoreTimes> core_times_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PROCFS_STAT_CPU_PARSER_H_
