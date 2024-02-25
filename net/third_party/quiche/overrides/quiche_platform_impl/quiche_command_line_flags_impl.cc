// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche_platform_impl/quiche_command_line_flags_impl.h"

#include <initializer_list>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/export_template.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_logging.h"

namespace quiche {

namespace {

size_t FindLineWrapPosition(const std::string& s, size_t desired_len) {
  if (s.length() <= desired_len) {
    return std::string::npos;
  }
  size_t pos = s.find_last_of(base::kWhitespaceASCII, desired_len);
  if (pos != std::string::npos) {
    return pos;
  }
  pos = s.find_first_of(base::kWhitespaceASCII, desired_len);
  if (pos != std::string::npos) {
    return pos;
  }
  return std::string::npos;
}

// Pretty-print a flag description in the format:
//
// --flag_name      Some text describing the flag that can
//                  wrap around to the next line.
void AppendFlagDescription(const std::string& name,
                           std::string help,
                           std::string* out) {
  const int kStartCol = 20;
  const int kEndCol = 80;
  const int kMinPadding = 2;
  static const char kDashes[] = "--";

  base::StrAppend(out, {kDashes, name});
  int col = strlen(kDashes) + name.length();
  if (col + kMinPadding < kEndCol) {
    // Start help text on same line
    int pad_len = std::max(kMinPadding, kStartCol - col);
    base::StrAppend(out, {std::string(pad_len, ' ')});
    col += pad_len;
  } else {
    // Start help text on next line
    base::StrAppend(out, {"\n", std::string(kStartCol, ' ')});
    col = kStartCol;
  }

  while (!help.empty()) {
    size_t desired_len = kEndCol - col;
    size_t wrap_pos = FindLineWrapPosition(help, desired_len);
    if (wrap_pos == std::string::npos) {
      base::StrAppend(out, {help});
      break;
    }
    base::StrAppend(
        out, {help.substr(0, wrap_pos), "\n", std::string(kStartCol, ' ')});
    help = help.substr(wrap_pos + 1);
    col = kStartCol;
  }
  base::StrAppend(out, {"\n"});
}

// Overload for platforms where base::CommandLine::StringType == std::string.
[[maybe_unused]] std::vector<std::string> ToQuicheStringVector(
    const std::vector<std::string>& v) {
  return v;
}

#if defined(WCHAR_T_IS_16_BIT)
// Overload for platforms where base::CommandLine::StringType == std::wstring.
[[maybe_unused]] std::vector<std::string> ToQuicheStringVector(
    const std::vector<std::wstring>& v) {
  std::vector<std::string> qsv;
  for (const auto& s : v) {
    if (!base::IsStringASCII(s)) {
      QUIC_LOG(ERROR) << "Unable to convert to ASCII: " << s;
      continue;
    }
    qsv.push_back(base::WideToASCII(s));
  }
  return qsv;
}
#endif  // defined(WCHAR_T_IS_16_BIT)

}  // namespace

// static
QuicheFlagRegistry& QuicheFlagRegistry::GetInstance() {
  static base::NoDestructor<QuicheFlagRegistry> instance;
  return *instance;
}

void QuicheFlagRegistry::RegisterFlag(
    const char* name,
    std::unique_ptr<QuicheFlagHelper> helper) {
  flags_.emplace(std::string(name), std::move(helper));
}

bool QuicheFlagRegistry::SetFlags(const base::CommandLine& command_line,
                                  std::string* error_msg) const {
  for (const auto& kv : flags_) {
    const std::string& name = kv.first;
    const QuicheFlagHelper* helper = kv.second.get();
    if (!command_line.HasSwitch(name)) {
      continue;
    }
    std::string value = command_line.GetSwitchValueASCII(name);
    if (!helper->SetFlag(value)) {
      *error_msg =
          base::StrCat({"Invalid value \"", value, "\" for flag --", name});
      return false;
    }
    QUIC_LOG(INFO) << "Set flag --" << name << " = " << value;
  }
  return true;
}

void QuicheFlagRegistry::ResetFlags() const {
  for (const auto& kv : flags_) {
    kv.second->ResetFlag();
    QUIC_LOG(INFO) << "Reset flag --" << kv.first;
  }
}

std::string QuicheFlagRegistry::GetHelp() const {
  std::string help;
  AppendFlagDescription("help", "Print this help message.", &help);
  for (const auto& kv : flags_) {
    AppendFlagDescription(kv.first, kv.second->GetHelp(), &help);
  }
  return help;
}

template <>
bool TypedQuicheFlagHelper<bool>::SetFlag(const std::string& s) const {
  static const base::NoDestructor<std::set<std::string>> kTrueValues(
      std::initializer_list<std::string>({"", "1", "t", "true", "y", "yes"}));
  static const base::NoDestructor<std::set<std::string>> kFalseValues(
      std::initializer_list<std::string>({"0", "f", "false", "n", "no"}));
  if (kTrueValues->find(base::ToLowerASCII(s)) != kTrueValues->end()) {
    *flag_ = true;
    return true;
  }
  if (kFalseValues->find(base::ToLowerASCII(s)) != kFalseValues->end()) {
    *flag_ = false;
    return true;
  }
  return false;
}

template <>
bool TypedQuicheFlagHelper<uint16_t>::SetFlag(const std::string& s) const {
  int value;
  if (!base::StringToInt(s, &value) ||
      value < std::numeric_limits<uint16_t>::min() ||
      value > std::numeric_limits<uint16_t>::max()) {
    return false;
  }
  *flag_ = static_cast<uint16_t>(value);
  return true;
}

template <>
bool TypedQuicheFlagHelper<int32_t>::SetFlag(const std::string& s) const {
  int32_t value;
  if (!base::StringToInt(s, &value)) {
    return false;
  }
  *flag_ = value;
  return true;
}

template <>
bool TypedQuicheFlagHelper<std::string>::SetFlag(const std::string& s) const {
  *flag_ = s;
  return true;
}

template class TypedQuicheFlagHelper<bool>;
template class TypedQuicheFlagHelper<uint16_t>;
template class TypedQuicheFlagHelper<int32_t>;
template class TypedQuicheFlagHelper<std::string>;

QuicheFlagRegistry::QuicheFlagRegistry() = default;
QuicheFlagRegistry::~QuicheFlagRegistry() = default;

std::vector<std::string> QuicheParseCommandLineFlagsImpl(
    const char* usage,
    int argc,
    const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  auto result = QuicheParseCommandLineFlagsHelper(
      usage, *base::CommandLine::ForCurrentProcess());
  if (result.exit_status.has_value()) {
    exit(*result.exit_status);
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_STDERR;
  CHECK(logging::InitLogging(settings));

  return result.non_flag_args;
}

QuicheParseCommandLineFlagsResult QuicheParseCommandLineFlagsHelper(
    const char* usage,
    const base::CommandLine& command_line) {
  QuicheParseCommandLineFlagsResult result;
  result.non_flag_args = ToQuicheStringVector(command_line.GetArgs());
  if (command_line.HasSwitch("h") || command_line.HasSwitch("help")) {
    QuichePrintCommandLineFlagHelpImpl(usage);
    result.exit_status = 0;
  } else {
    std::string msg;
    if (!QuicheFlagRegistry::GetInstance().SetFlags(command_line, &msg)) {
      std::cerr << msg << std::endl;
      result.exit_status = 1;
    }
  }
  return result;
}

void QuichePrintCommandLineFlagHelpImpl(const char* usage) {
  std::cout << usage << std::endl
            << "Options:" << std::endl
            << QuicheFlagRegistry::GetInstance().GetHelp() << std::endl;
}

QuicheParseCommandLineFlagsResult::QuicheParseCommandLineFlagsResult() =
    default;
QuicheParseCommandLineFlagsResult::QuicheParseCommandLineFlagsResult(
    const QuicheParseCommandLineFlagsResult&) = default;
QuicheParseCommandLineFlagsResult::~QuicheParseCommandLineFlagsResult() =
    default;

}  // namespace quiche
