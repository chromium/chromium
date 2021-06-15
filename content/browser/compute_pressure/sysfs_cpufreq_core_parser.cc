// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/sysfs_cpufreq_core_parser.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace content {

constexpr char SysfsCpufreqCoreParser::kSysfsCpuPath[];

// static
base::FilePath SysfsCpufreqCoreParser::CorePath(int core_id,
                                                const char* sysfs_cpu_path) {
  static constexpr char kCpuFreqDir[] = "/cpufreq";

  std::string core_id_string = base::NumberToString(core_id);
  std::string path_string =
      base::StrCat({base::StringPiece(sysfs_cpu_path), core_id_string,
                    base::StringPiece(kCpuFreqDir)});
  return base::FilePath(path_string);
}

SysfsCpufreqCoreParser::SysfsCpufreqCoreParser(const base::FilePath& core_path)
    : max_frequency_reader_(core_path.AppendASCII("cpuinfo_max_freq"),
                            core_path.AppendASCII("scaling_max_freq")),
      min_frequency_reader_(core_path.AppendASCII("cpuinfo_min_freq"),
                            core_path.AppendASCII("scaling_min_freq")),
      current_frequency_reader_(core_path.AppendASCII("cpuinfo_cur_freq"),
                                core_path.AppendASCII("scaling_cur_freq")),
      base_frequency_reader_(core_path.AppendASCII("base_frequency")) {}

SysfsCpufreqCoreParser::~SysfsCpufreqCoreParser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int64_t SysfsCpufreqCoreParser::ReadMaxFrequency() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return max_frequency_reader_.ReadNumber();
}

int64_t SysfsCpufreqCoreParser::ReadMinFrequency() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return min_frequency_reader_.ReadNumber();
}

int64_t SysfsCpufreqCoreParser::ReadBaseFrequency() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base_frequency_reader_.ReadNumber();
}

int64_t SysfsCpufreqCoreParser::ReadCurrentFrequency() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_frequency_reader_.ReadNumber();
}

SysfsCpufreqCoreParser::FileReader::FileReader(base::FilePath file_path)
    : file_path_(file_path) {
  DCHECK(!file_path_.empty());
}

SysfsCpufreqCoreParser::FileReader::~FileReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int64_t SysfsCpufreqCoreParser::FileReader::ReadNumber() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string file_bytes;
  if (!base::ReadFileToString(file_path_, &file_bytes))
    return -1;

  // The sysfs files have a trailing newline.
  base::StringPiece trimmed_file_bytes =
      base::TrimWhitespaceASCII(file_bytes, base::TrimPositions::TRIM_TRAILING);

  int64_t file_value;
  if (!base::StringToInt64(trimmed_file_bytes, &file_value) || file_value < 0)
    return -1;

  // CPUfreq reports frequencies in kHz.
  constexpr int kKhz = 1000;
  return file_value * kKhz;
}

SysfsCpufreqCoreParser::SysfsFileReader::SysfsFileReader(
    base::FilePath firmware_path,
    base::FilePath governor_path)
    : firmware_value_reader_(std::move(firmware_path)),
      governor_value_reader_(std::move(governor_path)) {}

SysfsCpufreqCoreParser::SysfsFileReader::~SysfsFileReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int64_t SysfsCpufreqCoreParser::SysfsFileReader::ReadNumber() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t file_value = firmware_value_reader_.ReadNumber();
  if (file_value != -1)
    return file_value;

  file_value = governor_value_reader_.ReadNumber();
  return file_value;
}

}  // namespace content
