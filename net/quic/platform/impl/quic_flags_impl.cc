// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_flags_impl.h"

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <set>

#include "base/command_line.h"
#include "base/export_template.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"

#define QUIC_FLAG(type, flag, value) type flag = value;
#include "net/quic/quic_flags_list.h"
#undef QUIC_FLAG

namespace quic {

namespace {

// Overload for platforms where base::CommandLine::StringType == std::string.
std::vector<std::string> __attribute__((unused))
ToQuicStringVector(const std::vector<std::string>& v) {
  return v;
}

// Overload for platforms where base::CommandLine::StringType == base::string16.
std::vector<std::string> __attribute__((unused))
ToQuicStringVector(const std::vector<base::string16>& v) {
  std::vector<std::string> qsv;
  for (const auto& s : v) {
    if (!base::IsStringASCII(s)) {
      QUIC_LOG(ERROR) << "Unable to convert to ASCII: " << s;
      continue;
    }
    qsv.push_back(base::UTF16ToASCII(s));
  }
  return qsv;
}

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

}  // namespace

// static
QuicFlagRegistry& QuicFlagRegistry::GetInstance() {
  static base::NoDestructor<QuicFlagRegistry> instance;
  return *instance;
}

void QuicFlagRegistry::RegisterFlag(const char* name,
                                    std::unique_ptr<QuicFlagHelper> helper) {
  flags_.emplace(std::string(name), std::move(helper));
}

bool QuicFlagRegistry::SetFlags(const base::CommandLine& command_line,
                                std::string* error_msg) const {
  for (const auto& kv : flags_) {
    const std::string& name = kv.first;
    const QuicFlagHelper* helper = kv.second.get();
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

void QuicFlagRegistry::ResetFlags() const {
  for (const auto& kv : flags_) {
    kv.second->ResetFlag();
    QUIC_LOG(INFO) << "Reset flag --" << kv.first;
  }
}

std::string QuicFlagRegistry::GetHelp() const {
  std::string help;
  AppendFlagDescription("help", "Print this help message.", &help);
  for (const auto& kv : flags_) {
    AppendFlagDescription(kv.first, kv.second->GetHelp(), &help);
  }
  return help;
}

template <>
bool TypedQuicFlagHelper<bool>::SetFlag(const std::string& s) const {
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
bool TypedQuicFlagHelper<int32_t>::SetFlag(const std::string& s) const {
  int32_t value;
  if (!base::StringToInt(s, &value)) {
    return false;
  }
  *flag_ = value;
  return true;
}

template <>
bool TypedQuicFlagHelper<std::string>::SetFlag(const std::string& s) const {
  *flag_ = s;
  return true;
}

template class EXPORT_TEMPLATE_DEFINE(QUIC_EXPORT_PRIVATE)
    TypedQuicFlagHelper<bool>;
template class EXPORT_TEMPLATE_DEFINE(QUIC_EXPORT_PRIVATE)
    TypedQuicFlagHelper<int32_t>;
template class EXPORT_TEMPLATE_DEFINE(QUIC_EXPORT_PRIVATE)
    TypedQuicFlagHelper<std::string>;

std::vector<std::string> QuicParseCommandLineFlagsImpl(
    const char* usage,
    int argc,
    const char* const* argv) {
  base::CommandLine::Init(argc, argv);
  auto result = QuicParseCommandLineFlagsHelper(
      usage, *base::CommandLine::ForCurrentProcess());
  if (result.exit_status.has_value()) {
    exit(*result.exit_status);
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_STDERR;
  CHECK(logging::InitLogging(settings));

  return result.non_flag_args;
}

QuicParseCommandLineFlagsResult QuicParseCommandLineFlagsHelper(
    const char* usage,
    const base::CommandLine& command_line) {
  QuicParseCommandLineFlagsResult result;
  result.non_flag_args = ToQuicStringVector(command_line.GetArgs());
  if (command_line.HasSwitch("h") || command_line.HasSwitch("help")) {
    QuicPrintCommandLineFlagHelpImpl(usage);
    result.exit_status = 0;
  } else {
    std::string msg;
    if (!QuicFlagRegistry::GetInstance().SetFlags(command_line, &msg)) {
      std::cerr << msg << std::endl;
      result.exit_status = 1;
    }
  }
  return result;
}

void QuicPrintCommandLineFlagHelpImpl(const char* usage) {
  std::cout << usage << std::endl
            << "Options:" << std::endl
            << QuicFlagRegistry::GetInstance().GetHelp() << std::endl;
}

QuicFlagRegistry::QuicFlagRegistry() = default;
QuicFlagRegistry::~QuicFlagRegistry() = default;
QuicParseCommandLineFlagsResult::QuicParseCommandLineFlagsResult() = default;
QuicParseCommandLineFlagsResult::QuicParseCommandLineFlagsResult(
    const QuicParseCommandLineFlagsResult&) = default;
QuicParseCommandLineFlagsResult::~QuicParseCommandLineFlagsResult() = default;

}  // namespace quic

namespace {

void SetQuicFlagByName_bool(bool* flag, const std::string& value) {
  if (value == "true" || value == "True")
    *flag = true;
  else if (value == "false" || value == "False")
    *flag = false;
}
void SetQuicFlagByName_double(double* flag, const std::string& value) {
  double val;
  if (base::StringToDouble(value, &val))
    *flag = val;
}

void SetQuicFlagByName_uint32_t(uint32_t* flag, const std::string& value) {
  int val;
  if (base::StringToInt(value, &val) && val >= 0)
    *flag = val;
}

void SetQuicFlagByName_int32_t(int32_t* flag, const std::string& value) {
  int val;
  if (base::StringToInt(value, &val))
    *flag = val;
}

void SetQuicFlagByName_int64_t(int64_t* flag, const std::string& value) {
  int64_t val;
  if (base::StringToInt64(value, &val))
    *flag = val;
}

}  // namespace

void SetQuicFlagByName(const std::string& flag_name, const std::string& value) {
#define QUIC_FLAG(type, flag, default_value) \
  if (flag_name == #flag) {                  \
    SetQuicFlagByName_##type(&flag, value);  \
    return;                                  \
  }
#include "net/quic/quic_flags_list.h"
#undef QUIC_FLAG
}
