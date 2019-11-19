// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/browser/api/declarative_net_request/filter_list_converter/converter.h"

namespace {

const char kSwitchInputFilterlistFiles[] = "input_filterlists";
const char kSwitchOutputPath[] = "output_path";
const char kSwitchOutputType[] = "output_type";
const char kOutputTypeExtension[] = "extension";
const char kOutputTypeJSON[] = "json";
const base::FilePath::CharType kJSONExtension[] = FILE_PATH_LITERAL(".json");

const char kHelpMsg[] = R"(
  filter_list_converter --input_filterlists=[<path1>, <path2>]
          --output_path=<path> --output_type=<extension,json>

  Filter List Converter is a tool to convert filter list files in the text
  format to a JSON file in a format supported by the Declarative Net Request
  API. It can either output the complete extension or just the JSON ruleset.

  --input_filterlists = List of input paths to text filter list files.
  --output_path = The output path. The parent directory should exist.
  --output_type = Optional switch. One of "extension" or "json". "json" is the
                  default.
)";

namespace filter_list_converter =
    extensions::declarative_net_request::filter_list_converter;

void PrintHelp() {
  LOG(ERROR) << kHelpMsg;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(kSwitchInputFilterlistFiles) ||
      !command_line.HasSwitch(kSwitchOutputPath)) {
    PrintHelp();
    return 1;
  }

  std::vector<base::FilePath> input_paths;
  base::CommandLine::StringType comma_separated_paths =
      command_line.GetSwitchValueNative(kSwitchInputFilterlistFiles);

#if defined(OS_WIN)
  base::CommandLine::StringType separator = base::ASCIIToUTF16(",");
#else
  base::CommandLine::StringType separator(",");
#endif

  for (const auto& piece : base::SplitStringPiece(
           comma_separated_paths, separator, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path(piece);

    if (!base::PathExists(path)) {
      LOG(ERROR) << "Input path " << piece << " does not exist.";
      return 1;
    }

    input_paths.push_back(path);
  }
  if (input_paths.empty()) {
    LOG(ERROR) << base::StringPrintf(
        "No valid input files specified using '%s'.",
        kSwitchInputFilterlistFiles);
    return 1;
  }

  filter_list_converter::WriteType write_type =
      filter_list_converter::kJSONRuleset;
  if (command_line.HasSwitch(kSwitchOutputType)) {
    std::string output_type =
        command_line.GetSwitchValueASCII(kSwitchOutputType);
    if (output_type == kOutputTypeExtension) {
      write_type = filter_list_converter::kExtension;
    } else if (output_type == kOutputTypeJSON) {
      write_type = filter_list_converter::kJSONRuleset;
    } else {
      LOG(ERROR) << base::StringPrintf("Invalid value for switch '%s'",
                                       kSwitchOutputType);
      return 1;
    }
  }

  base::FilePath output_path =
      command_line.GetSwitchValuePath(kSwitchOutputPath);
  bool invalid_output_path = false;
  switch (write_type) {
    case filter_list_converter::kExtension:
      invalid_output_path = !base::DirectoryExists(output_path);
      break;
    case filter_list_converter::kJSONRuleset:
      invalid_output_path = output_path.Extension() != kJSONExtension;
      invalid_output_path |= !base::DirectoryExists(output_path.DirName());
      break;
  }
  if (invalid_output_path) {
    LOG(ERROR) << "Invalid output path " << output_path.value();
    return 1;
  }

  if (!filter_list_converter::ConvertRuleset(input_paths, output_path,
                                             write_type)) {
    LOG(ERROR) << "Conversion failed.";
    return 1;
  }

  return 0;
}
