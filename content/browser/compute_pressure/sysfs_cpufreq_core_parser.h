// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_SYSFS_CPUFREQ_CORE_PARSER_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_SYSFS_CPUFREQ_CORE_PARSER_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"

namespace content {

// Parses per-core information reported by the Linux kernel's CPUFreq subsystem.
//
// The CPUFreq subsystem is available since the 2.6.15.6 kernel, and can be
// enabled or disabled at build time. CPUfreq is enabled on Chrome OS, Android,
// and all popular Linux distributions. An overview is available at
// https://www.kernel.org/doc/html/latest/admin-guide/pm/cpufreq.html
//
// CPUFreq exposes CPU scaling information via sysfs. The interface is
// documented at
// https://www.kernel.org/doc/Documentation/cpu-freq/user-guide.txt
class CONTENT_EXPORT SysfsCpufreqCoreParser {
 public:
  // The "production" `sysfs_cpu_path` value for CorePath().
  static constexpr base::FilePath::CharType kSysfsCpuPath[] =
      FILE_PATH_LITERAL("/sys/devices/system/cpu/cpu");

  // The directory where CPUFreq reports the desired core's information.
  //
  // `core_id` must be non-negative. `core_id` does not need to represent an
  // existing / online core.
  //
  // `sysfs_root_path` is exposed for testing. Production code should pass in
  // kSysfsRootPath.
  static base::FilePath CorePath(
      int core_id,
      const base::FilePath::CharType* sysfs_cpu_path);

  // `core_path` must point to the directory where CPUFreq reports the desired
  // core's information.
  explicit SysfsCpufreqCoreParser(const base::FilePath& core_path);

  ~SysfsCpufreqCoreParser();

  SysfsCpufreqCoreParser(const SysfsCpufreqCoreParser&) = delete;
  SysfsCpufreqCoreParser& operator=(const SysfsCpufreqCoreParser&) = delete;

  // Returns -1 if reading the information failed for any reason.
  int64_t ReadMaxFrequency();

  // Returns -1 if reading the information failed for any reason.
  int64_t ReadMinFrequency();

  // Returns -1 if reading the information failed for any reason.
  //
  // If this method fails, retries are unlikely to succeed.
  //
  // The base frequency is exposed by some CPUfreq scaling drivers, when the
  // CPUs they target is present. For this reason, this method may fail on cores
  // where all the other methods have succeeded.
  //
  // For example, the intel_pstate driver, which is enabled on Chrome OS,
  // exposes the base frequency for processors that support Hardware P-State
  // (a.k.a. "HWP" and "Intel Speed Shift"). The intel_pstate driver is
  // documented at
  // https://www.kernel.org/doc/html/latest/admin-guide/pm/intel_pstate.html
  int64_t ReadBaseFrequency();

  // Returns -1 if reading the information failed for any reason.
  int64_t ReadCurrentFrequency();

 private:
  // Helper that reads information from a file.
  class FileReader {
   public:
    explicit FileReader(base::FilePath file_path);
    ~FileReader();

    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;

    // Reads the file's contents, which must be a base-10 non-negative integer.
    //
    // Returns -1 if reading the file failed, or if the file contents was not
    // parsed as a non-negative integer.
    int64_t ReadNumber();

   private:
    SEQUENCE_CHECKER(sequence_checker_);

    const base::FilePath file_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  };

  // Helper that reads information from two alternative places.
  class SysfsFileReader {
   public:
    // `firmware_path` is the primary source of information. The information
    // there is reported, as long as the file exists, and its contents parses
    // correctly.
    //
    // `governor_path` is the backup information source.
    SysfsFileReader(base::FilePath firmware_path, base::FilePath governor_path);
    ~SysfsFileReader();

    SysfsFileReader(const SysfsFileReader&) = delete;
    SysfsFileReader& operator=(const SysfsFileReader&) = delete;

    // Reads out the numeric information from the given source.
    //
    // Returns -1 if reading the information failed for any reason.
    int64_t ReadNumber();

   private:
    SEQUENCE_CHECKER(sequence_checker_);

    // Reads the information provided by the device's firmware.
    FileReader firmware_value_reader_ GUARDED_BY_CONTEXT(sequence_checker_);
    // Reads the information provided by the CPUfreq governor.
    FileReader governor_value_reader_ GUARDED_BY_CONTEXT(sequence_checker_);
  };

  SEQUENCE_CHECKER(sequence_checker_);

  // Reads for the core's maximum frequency.
  SysfsFileReader max_frequency_reader_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Reads for the core's minimum frequency.
  SysfsFileReader min_frequency_reader_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Reads for the core's current frequency.
  SysfsFileReader current_frequency_reader_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Reads the base frequency provided by the CPUfreq driver.
  FileReader base_frequency_reader_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_SYSFS_CPUFREQ_CORE_PARSER_H_
