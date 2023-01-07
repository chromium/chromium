//
// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/log/internal/test_actions.h"

#include <cassert>
#include <iostream>
#include <string>

#include "absl/base/config.h"
#include "absl/log/internal/config.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace log_internal {

void WriteToStderrWithFilename::operator()(const absl::LogEntry& entry) const {
  std::cerr << message << " (file: " << entry.source_filename() << ")"
            << std::endl;
}

void WriteEntryToStderr::operator()(const absl::LogEntry& entry) const {
  if (!message.empty()) std::cerr << message << std::endl;

  std::cerr << "LogEntry{\n"
            << "  source_filename: \""
            << absl::CHexEscape(entry.source_filename()) << "\"\n"
            << "  source_basename: \""
            << absl::CHexEscape(entry.source_basename()) << "\"\n"
            << "  source_line: " << entry.source_line() << "\n"
            << "  prefix: " << (entry.prefix() ? "true\n" : "false\n")
            << "  log_severity: " << entry.log_severity() << "\n"
            << "  timestamp: " << entry.timestamp() << "\n"
            << "  text_message: \"" << absl::CHexEscape(entry.text_message())
            << "\"\n  verbosity: " << entry.verbosity() << "\n"
            << "}" << std::endl;
}

void WriteEntryToStderr::operator()(absl::LogSeverity severity,
                                    absl::string_view filename,
                                    absl::string_view log_message) const {
  if (!message.empty()) std::cerr << message << std::endl;

  std::cerr << "LogEntry{\n"
            << "  source_filename: \"" << absl::CHexEscape(filename) << "\"\n"
            << "  log_severity: " << severity << "\n"
            << "  text_message: \"" << absl::CHexEscape(log_message) << "}"
            << std::endl;
}

}  // namespace log_internal
ABSL_NAMESPACE_END
}  // namespace absl
