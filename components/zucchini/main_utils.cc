// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/main_utils.h"

#include <stddef.h>

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/zucchini/io_utils.h"
#include "components/zucchini/version_info.h"
#include "components/zucchini/zucchini_commands.h"

#if BUILDFLAG(IS_WIN)
// clang-format off
#include <windows.h> // Must be in front of other Windows header files.
// clang-format on

#include <psapi.h>
#endif

namespace {

/******** Command ********/

// Specifications for a Zucchini command.
struct Command {
  constexpr Command(const char* name_in,
                    const char* usage_in,
                    int num_args_in,
                    CommandFunction command_function_in)
      : name(name_in),
        usage(usage_in),
        num_args(num_args_in),
        command_function(command_function_in) {}
  Command(const Command&) = default;
  ~Command() = default;

  // Unique name of command. |-name| is used to select from command-line.
  const char* const name;

  // Usage help text of command.
  const char* const usage;

  // Number of arguments (assumed to be filenames) used by the command.
  const int num_args;

  // Main function to run for the command.
  const CommandFunction command_function;
};

/******** List of Zucchini commands ********/

constexpr Command kCommands[] = {
    {"gen",
     "-gen <old_file> <new_file> <patch_file> [-raw] [-keep]"
     " [-impose=#+#=#+#,#+#=#+#,...]",
     3, &MainGen},
    {"apply", "-apply <old_file> <patch_file> <new_file> [-keep]", 3,
     &MainApply},
    {"verify", "-verify <patch_file>", 1, &MainVerify},
    {"read", "-read <exe> [-dump]", 1, &MainRead},
    {"detect", "-detect <archive_file>", 1, &MainDetect},
    {"match", "-match <old_file> <new_file> [-impose=#+#=#+#,#+#=#+#,...]", 2,
     &MainMatch},
    {"crc32", "-crc32 <file>", 1, &MainCrc32},
    {"suffix-array", "-suffix-array <file>", 1, &MainSuffixArray},
};

/******** GetPeakMemoryMetrics ********/

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Linux does not have an exact mapping to the values used on Windows so use a
// close approximation:
// peak_virtual_memory ~= peak_page_file_usage
// resident_set_size_hwm (high water mark) ~= peak_working_set_size
//
// On failure the input values will be set to 0.
void GetPeakMemoryMetrics(size_t* peak_virtual_memory,
                          size_t* resident_set_size_hwm) {
  *peak_virtual_memory = 0;
  *resident_set_size_hwm = 0;
  auto status_path =
      base::FilePath("/proc")
          .Append(base::NumberToString(base::GetCurrentProcessHandle()))
          .Append("status");
  std::string contents_string;
  base::ReadFileToString(status_path, &contents_string);
  std::vector<std::string_view> lines = base::SplitStringPiece(
      contents_string, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const auto& line : lines) {
    // Tokens should generally be of the form "Metric: <val> kB"
    std::vector<std::string_view> tokens = base::SplitStringPiece(
        line, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (tokens.size() < 2)
      continue;

    if (tokens[0] == "VmPeak:") {
      if (base::StringToSizeT(tokens[1], peak_virtual_memory)) {
        *peak_virtual_memory *= 1024;  // in kiB
        if (*resident_set_size_hwm)
          return;
      }
    } else if (tokens[0] == "VmHWM:") {
      if (base::StringToSizeT(tokens[1], resident_set_size_hwm)) {
        *resident_set_size_hwm *= 1024;  // in kiB
        if (*peak_virtual_memory)
          return;
      }
    }
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
// On failure the input values will be set to 0.
void GetPeakMemoryMetrics(size_t* peak_page_file_usage,
                          size_t* peak_working_set_size) {
  *peak_page_file_usage = 0;
  *peak_working_set_size = 0;
  PROCESS_MEMORY_COUNTERS pmc;
  if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
    *peak_page_file_usage = pmc.PeakPagefileUsage;
    *peak_working_set_size = pmc.PeakWorkingSetSize;
  }
}
#endif  // BUILDFLAG(IS_WIN)

/******** ScopedResourceUsageTracker ********/

// A class to track and log system resource usage.
class ScopedResourceUsageTracker {
 public:
  // Initializes states for tracking.
  ScopedResourceUsageTracker() {
    start_time_ = base::TimeTicks::Now();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    GetPeakMemoryMetrics(&start_peak_page_file_usage_,
                         &start_peak_working_set_size_);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  }

  // Computes and prints usage.
  ~ScopedResourceUsageTracker() {
    base::TimeTicks end_time = base::TimeTicks::Now();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    size_t cur_peak_page_file_usage = 0;
    size_t cur_peak_working_set_size = 0;
    GetPeakMemoryMetrics(&cur_peak_page_file_usage, &cur_peak_working_set_size);

    LOG(INFO) << "Zucchini.PeakPagefileUsage "
              << cur_peak_page_file_usage / 1024 << " KiB";
    LOG(INFO) << "Zucchini.PeakPagefileUsageChange "
              << (cur_peak_page_file_usage - start_peak_page_file_usage_) / 1024
              << " KiB";
    LOG(INFO) << "Zucchini.PeakWorkingSetSize "
              << cur_peak_working_set_size / 1024 << " KiB";
    LOG(INFO) << "Zucchini.PeakWorkingSetSizeChange "
              << (cur_peak_working_set_size - start_peak_working_set_size_) /
                     1024
              << " KiB";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)

    LOG(INFO) << "Zucchini.TotalTime " << (end_time - start_time_).InSecondsF()
              << " s";
  }

 private:
  base::TimeTicks start_time_;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  size_t start_peak_page_file_usage_ = 0;
  size_t start_peak_working_set_size_ = 0;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
};

/******** Helper functions ********/

// Translates |command_line| arguments to a vector of base::FilePath (expecting
// exactly |expected_count|). On success, writes the results to |paths| and
// returns true. Otherwise returns false.
bool CheckAndGetFilePathParams(const base::CommandLine& command_line,
                               size_t expected_count,
                               std::vector<base::FilePath>* paths) {
  const base::CommandLine::StringVector& args = command_line.GetArgs();
  if (args.size() != expected_count)
    return false;

  paths->clear();
  paths->reserve(args.size());
  for (const auto& arg : args)
    paths->emplace_back(arg);
  return true;
}

// Prints main Zucchini usage text.
void PrintUsage(std::ostream& err) {
  err << "Version: " << zucchini::kMajorVersion << "."
      << zucchini::kMinorVersion << std::endl;
  err << "Usage:" << std::endl;
  for (const Command& command : kCommands)
    err << "  zucchini " << command.usage << std::endl;
}

}  // namespace

/******** Exported Functions ********/

zucchini::status::Code RunZucchiniCommand(const base::CommandLine& command_line,
                                          std::ostream& out,
                                          std::ostream& err) {
  // Look for a command with name that matches input.
  const Command* command_use = nullptr;
  for (const Command& command : kCommands) {
    if (command_line.HasSwitch(command.name)) {
      if (command_use) {        // Too many commands found.
        command_use = nullptr;  // Set to null to flag error.
        break;
      }
      command_use = &command;
    }
  }

  // Expect exactly 1 matching command. If 0 or >= 2, print usage and quit.
  if (!command_use) {
    err << "Must have exactly one of:" << std::endl;
    err << "  [";
    zucchini::PrefixSep sep(", ");
    for (const Command& command : kCommands)
      err << sep << "-" << command.name;
    err << "]" << std::endl;
    PrintUsage(err);
    return zucchini::status::kStatusInvalidParam;
  }

  // Try to parse filename arguments. On failure, print usage and quit.
  std::vector<base::FilePath> paths;
  if (!CheckAndGetFilePathParams(command_line, command_use->num_args, &paths)) {
    err << command_use->usage << std::endl;
    PrintUsage(err);
    return zucchini::status::kStatusInvalidParam;
  }

  ScopedResourceUsageTracker resource_usage_tracker;
  return command_use->command_function(
      {raw_ref(command_line), raw_ref(paths), raw_ref(out), raw_ref(err)});
}
