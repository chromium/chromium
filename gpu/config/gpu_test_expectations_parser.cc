// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_test_expectations_parser.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace gpu {

namespace {

enum LineParserStage {
  kLineParserBegin = 0,
  kLineParserBugID,
  kLineParserConfigs,
  kLineParserColon,
  kLineParserTestName,
  kLineParserEqual,
  kLineParserExpectations,
};

enum Token {
  // os
  kConfigWin10 = 0,
  kConfigWin,
  kConfigMacSnowLeopard,
  kConfigMacLion,
  kConfigMacMountainLion,
  kConfigMacMavericks,
  kConfigMacYosemite,
  kConfigMacElCapitan,
  kConfigMacSierra,
  kConfigMacHighSierra,
  kConfigMacMojave,
  kConfigMacCatalina,
  kConfigMacBigSur,
  kConfigMacMonterey,
  kConfigMacVentura,
  kConfigMacSonoma,
  kConfigMacSequoia,
  kConfigMac,
  kConfigLinux,
  kConfigChromeOS,
  kConfigAndroid,
  // gpu vendor
  kConfigNVidia,
  kConfigAMD,
  kConfigARM,
  kConfigIntel,
  kConfigVMWare,
  kConfigQualcomm,
  // build type
  kConfigRelease,
  kConfigDebug,
  // ANGLE renderer
  kConfigD3D9,
  kConfigD3D11,
  kConfigGLDesktop,
  kConfigGLES,
  // command decoder
  kConfigPassthrough,
  kConfigValidating,
  // expectation
  kExpectationPass,
  kExpectationFail,
  kExpectationFlaky,
  kExpectationTimeout,
  kExpectationSkip,
  // separator
  kSeparatorColon,
  kSeparatorEqual,

  kNumberOfExactMatchTokens,

  // others
  kConfigGPUDeviceID,
  kTokenComment,
  kTokenWord,
};

struct TokenInfo {
  const char* name;
  int32_t flag;
};

const std::array<TokenInfo, 42> kTokenData = {{
    {"win10", GPUTestConfig::kOsWin10},
    {"win", GPUTestConfig::kOsWin},
    {"snowleopard", GPUTestConfig::kOsMacSnowLeopard},
    {"lion", GPUTestConfig::kOsMacLion},
    {"mountainlion", GPUTestConfig::kOsMacMountainLion},
    {"mavericks", GPUTestConfig::kOsMacMavericks},
    {"yosemite", GPUTestConfig::kOsMacYosemite},
    {"elcapitan", GPUTestConfig::kOsMacElCapitan},
    {"sierra", GPUTestConfig::kOsMacSierra},
    {"highsierra", GPUTestConfig::kOsMacHighSierra},
    {"mojave", GPUTestConfig::kOsMacMojave},
    {"catalina", GPUTestConfig::kOsMacCatalina},
    {"bigsur", GPUTestConfig::kOsMacBigSur},
    {"monterey", GPUTestConfig::kOsMacMonterey},
    {"ventura", GPUTestConfig::kOsMacVentura},
    {"sonoma", GPUTestConfig::kOsMacSonoma},
    {"sequoia", GPUTestConfig::kOsMacSequoia},
    {"mac", GPUTestConfig::kOsMac},
    {"linux", GPUTestConfig::kOsLinux},
    {"chromeos", GPUTestConfig::kOsChromeOS},
    {"android", GPUTestConfig::kOsAndroid},
    {"nvidia", 0x10DE},
    {"amd", 0x1002},
    {"arm", 0x13b5},
    {"intel", 0x8086},
    {"vmware", 0x15ad},
    {"qualcomm", 0x5143},
    {"release", GPUTestConfig::kBuildTypeRelease},
    {"debug", GPUTestConfig::kBuildTypeDebug},
    {"d3d9", GPUTestConfig::kAPID3D9},
    {"d3d11", GPUTestConfig::kAPID3D11},
    {"opengl", GPUTestConfig::kAPIGLDesktop},
    {"gles", GPUTestConfig::kAPIGLES},
    {"passthrough", GPUTestConfig::kCommandDecoderPassthrough},
    {"validating", GPUTestConfig::kCommandDecoderValidating},
    {"pass", GPUTestExpectationsParser::kGpuTestPass},
    {"fail", GPUTestExpectationsParser::kGpuTestFail},
    {"flaky", GPUTestExpectationsParser::kGpuTestFlaky},
    {"timeout", GPUTestExpectationsParser::kGpuTestTimeout},
    {"skip", GPUTestExpectationsParser::kGpuTestSkip},
    {":", 0},
    {"=", 0},
}};

enum ErrorType {
  kErrorFileIO = 0,
  kErrorIllegalEntry,
  kErrorInvalidEntry,
  kErrorEntryWithOsConflicts,
  kErrorEntryWithGpuVendorConflicts,
  kErrorEntryWithBuildTypeConflicts,
  kErrorEntryWithAPIConflicts,
  kErrorEntryWithCommandDecoderConflicts,
  kErrorEntryWithGpuDeviceIdConflicts,
  kErrorEntryWithExpectationConflicts,
  kErrorEntriesOverlap,

  kNumberOfErrors,
};

const std::array<const char* const, 11> kErrorMessage = {
    "file IO failed",
    "entry with wrong format",
    "entry invalid, likely wrong modifiers combination",
    "entry with OS modifier conflicts",
    "entry with GPU vendor modifier conflicts",
    "entry with GPU build type conflicts",
    "entry with GPU API conflicts",
    "entry with GPU process command decoder conflicts",
    "entry with GPU device id conflicts or malformat",
    "entry with expectation modifier conflicts",
    "two entries' configs overlap",
};

Token ParseToken(const std::string& word) {
  if (base::StartsWith(word, "//", base::CompareCase::INSENSITIVE_ASCII))
    return kTokenComment;
  if (base::StartsWith(word, "0x", base::CompareCase::INSENSITIVE_ASCII))
    return kConfigGPUDeviceID;

  for (int32_t i = 0; i < kNumberOfExactMatchTokens; ++i) {
    if (base::EqualsCaseInsensitiveASCII(word, kTokenData[i].name))
      return static_cast<Token>(i);
  }
  return kTokenWord;
}

// reference name can have the last character as *.
bool NamesMatching(const std::string& ref, const std::string& test_name) {
  size_t len = ref.length();
  if (len == 0)
    return false;
  if (ref[len - 1] == '*') {
    if (test_name.length() > len -1 &&
        ref.compare(0, len - 1, test_name, 0, len - 1) == 0)
      return true;
    return false;
  }
  return (ref == test_name);
}

}  // namespace anonymous

GPUTestExpectationsParser::GPUTestExpectationsParser() {
  // Some sanity check.
  DCHECK_EQ(static_cast<unsigned int>(kNumberOfExactMatchTokens),
            sizeof(kTokenData) / sizeof(kTokenData[0]));
  DCHECK_EQ(static_cast<unsigned int>(kNumberOfErrors),
            sizeof(kErrorMessage) / sizeof(kErrorMessage[0]));
}

GPUTestExpectationsParser::~GPUTestExpectationsParser() = default;

bool GPUTestExpectationsParser::LoadTestExpectations(const std::string& data) {
  entries_.clear();
  error_messages_.clear();

  std::vector<std::string> lines = base::SplitString(
      data, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  bool rt = true;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!ParseLine(lines[i], i + 1))
      rt = false;
  }
  if (DetectConflictsBetweenEntries()) {
    entries_.clear();
    rt = false;
  }

  return rt;
}

bool GPUTestExpectationsParser::LoadTestExpectations(
    const base::FilePath& path) {
  entries_.clear();
  error_messages_.clear();

  std::string data;
  if (!base::ReadFileToString(path, &data)) {
    error_messages_.push_back(kErrorMessage[kErrorFileIO]);
    return false;
  }
  return LoadTestExpectations(data);
}

int32_t GPUTestExpectationsParser::GetTestExpectation(
    const std::string& test_name,
    const GPUTestBotConfig& bot_config) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (NamesMatching(entries_[i].test_name, test_name) &&
        bot_config.Matches(entries_[i].test_config))
      return entries_[i].test_expectation;
  }
  return kGpuTestPass;
}

const std::vector<std::string>&
GPUTestExpectationsParser::GetErrorMessages() const {
  return error_messages_;
}

bool GPUTestExpectationsParser::ParseConfig(
    const std::string& config_data, GPUTestConfig* config) {
  DCHECK(config);
  std::vector<std::string> tokens = base::SplitString(
      config_data, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);

  for (size_t i = 0; i < tokens.size(); ++i) {
    Token token = ParseToken(tokens[i]);
    switch (token) {
      case kConfigWin10:
      case kConfigWin:
      case kConfigMacSnowLeopard:
      case kConfigMacLion:
      case kConfigMacMountainLion:
      case kConfigMacMavericks:
      case kConfigMacYosemite:
      case kConfigMacElCapitan:
      case kConfigMacSierra:
      case kConfigMacHighSierra:
      case kConfigMacMojave:
      case kConfigMacCatalina:
      case kConfigMacBigSur:
      case kConfigMacMonterey:
      case kConfigMacVentura:
      case kConfigMacSonoma:
      case kConfigMacSequoia:
      case kConfigMac:
      case kConfigLinux:
      case kConfigChromeOS:
      case kConfigAndroid:
      case kConfigNVidia:
      case kConfigAMD:
      case kConfigARM:
      case kConfigIntel:
      case kConfigVMWare:
      case kConfigQualcomm:
      case kConfigRelease:
      case kConfigDebug:
      case kConfigD3D9:
      case kConfigD3D11:
      case kConfigGLDesktop:
      case kConfigGLES:
      case kConfigPassthrough:
      case kConfigValidating:
      case kConfigGPUDeviceID:
        if (token == kConfigGPUDeviceID) {
          if (!UpdateTestConfig(config, tokens[i], 0))
            return false;
        } else {
          if (!UpdateTestConfig(config, token, 0))
            return false;
        }
        break;
      default:
        return false;
    }
  }
  return true;
}

bool GPUTestExpectationsParser::ParseLine(
    const std::string& line_data, size_t line_number) {
  std::vector<std::string> tokens = base::SplitString(
      line_data, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  int32_t stage = kLineParserBegin;
  GPUTestExpectationEntry entry;
  entry.line_number = line_number;
  GPUTestConfig& config = entry.test_config;
  bool comments_encountered = false;
  for (size_t i = 0; i < tokens.size() && !comments_encountered; ++i) {
    Token token = ParseToken(tokens[i]);
    switch (token) {
      case kTokenComment:
        comments_encountered = true;
        break;
      case kConfigWin10:
      case kConfigWin:
      case kConfigMacSnowLeopard:
      case kConfigMacLion:
      case kConfigMacMountainLion:
      case kConfigMacMavericks:
      case kConfigMacYosemite:
      case kConfigMacElCapitan:
      case kConfigMacSierra:
      case kConfigMacHighSierra:
      case kConfigMacMojave:
      case kConfigMacCatalina:
      case kConfigMacBigSur:
      case kConfigMacMonterey:
      case kConfigMacVentura:
      case kConfigMacSonoma:
      case kConfigMacSequoia:
      case kConfigMac:
      case kConfigLinux:
      case kConfigChromeOS:
      case kConfigAndroid:
      case kConfigNVidia:
      case kConfigAMD:
      case kConfigARM:
      case kConfigIntel:
      case kConfigVMWare:
      case kConfigQualcomm:
      case kConfigRelease:
      case kConfigDebug:
      case kConfigD3D9:
      case kConfigD3D11:
      case kConfigGLDesktop:
      case kConfigGLES:
      case kConfigPassthrough:
      case kConfigValidating:
      case kConfigGPUDeviceID:
        // MODIFIERS, could be in any order, need at least one.
        if (stage != kLineParserConfigs && stage != kLineParserBugID) {
          PushErrorMessage(kErrorMessage[kErrorIllegalEntry],
                           line_number);
          return false;
        }
        if (token == kConfigGPUDeviceID) {
          if (!UpdateTestConfig(&config, tokens[i], line_number))
            return false;
        } else {
          if (!UpdateTestConfig(&config, token, line_number))
            return false;
        }
        if (stage == kLineParserBugID)
          stage++;
        break;
      case kSeparatorColon:
        // :
        if (stage != kLineParserConfigs) {
          PushErrorMessage(kErrorMessage[kErrorIllegalEntry],
                           line_number);
          return false;
        }
        stage++;
        break;
      case kSeparatorEqual:
        // =
        if (stage != kLineParserTestName) {
          PushErrorMessage(kErrorMessage[kErrorIllegalEntry],
                           line_number);
          return false;
        }
        stage++;
        break;
      case kTokenWord:
        // BUG_ID or TEST_NAME
        if (stage == kLineParserBegin) {
          // Bug ID is not used for anything; ignore it.
        } else if (stage == kLineParserColon) {
          entry.test_name = tokens[i];
        } else {
          PushErrorMessage(kErrorMessage[kErrorIllegalEntry],
                           line_number);
          return false;
        }
        stage++;
        break;
      case kExpectationPass:
      case kExpectationFail:
      case kExpectationFlaky:
      case kExpectationTimeout:
      case kExpectationSkip:
        // TEST_EXPECTATIONS
        if (stage != kLineParserEqual && stage != kLineParserExpectations) {
          PushErrorMessage(kErrorMessage[kErrorIllegalEntry],
                           line_number);
          return false;
        }
        if ((kTokenData[token].flag & entry.test_expectation) != 0) {
          PushErrorMessage(kErrorMessage[kErrorEntryWithExpectationConflicts],
                           line_number);
          return false;
        }
        entry.test_expectation =
            (kTokenData[token].flag | entry.test_expectation);
        if (stage == kLineParserEqual)
          stage++;
        break;
      default:
        DCHECK(false);
        break;
    }
  }
  if (stage == kLineParserBegin) {
    // The whole line is empty or all comments
    return true;
  }
  if (stage == kLineParserExpectations) {
    if (!config.IsValid()) {
        PushErrorMessage(kErrorMessage[kErrorInvalidEntry], line_number);
        return false;
    }
    entries_.push_back(entry);
    return true;
  }
  PushErrorMessage(kErrorMessage[kErrorIllegalEntry], line_number);
  return false;
}

bool GPUTestExpectationsParser::UpdateTestConfig(GPUTestConfig* config,
                                                 int32_t token,
                                                 size_t line_number) {
  DCHECK(config);
  switch (token) {
    case kConfigWin10:
    case kConfigWin:
    case kConfigMacSnowLeopard:
    case kConfigMacLion:
    case kConfigMacMountainLion:
    case kConfigMacMavericks:
    case kConfigMacYosemite:
    case kConfigMacElCapitan:
    case kConfigMacSierra:
    case kConfigMacHighSierra:
    case kConfigMacMojave:
    case kConfigMacCatalina:
    case kConfigMacBigSur:
    case kConfigMacMonterey:
    case kConfigMacVentura:
    case kConfigMacSonoma:
    case kConfigMacSequoia:
    case kConfigMac:
    case kConfigLinux:
    case kConfigChromeOS:
    case kConfigAndroid:
      if ((config->os() & kTokenData[token].flag) != 0) {
        PushErrorMessage(kErrorMessage[kErrorEntryWithOsConflicts],
                         line_number);
        return false;
      }
      config->set_os(config->os() | kTokenData[token].flag);
      break;
    case kConfigNVidia:
    case kConfigAMD:
    case kConfigARM:
    case kConfigIntel:
    case kConfigVMWare:
    case kConfigQualcomm: {
      uint32_t gpu_vendor = static_cast<uint32_t>(kTokenData[token].flag);
        for (size_t i = 0; i < config->gpu_vendor().size(); ++i) {
          if (config->gpu_vendor()[i] == gpu_vendor) {
            PushErrorMessage(
                kErrorMessage[kErrorEntryWithGpuVendorConflicts],
                line_number);
            return false;
          }
        }
        config->AddGPUVendor(gpu_vendor);
    } break;
    case kConfigRelease:
    case kConfigDebug:
      if ((config->build_type() & kTokenData[token].flag) != 0) {
        PushErrorMessage(
            kErrorMessage[kErrorEntryWithBuildTypeConflicts],
            line_number);
        return false;
      }
      config->set_build_type(
          config->build_type() | kTokenData[token].flag);
      break;
    case kConfigD3D9:
    case kConfigD3D11:
    case kConfigGLDesktop:
    case kConfigGLES:
      if ((config->api() & kTokenData[token].flag) != 0) {
        PushErrorMessage(kErrorMessage[kErrorEntryWithAPIConflicts],
                         line_number);
        return false;
      }
      config->set_api(config->api() | kTokenData[token].flag);
      break;
    case kConfigPassthrough:
    case kConfigValidating:
      if ((config->command_decoder() & kTokenData[token].flag) != 0) {
        PushErrorMessage(kErrorMessage[kErrorEntryWithCommandDecoderConflicts],
                         line_number);
        return false;
      }
      config->set_command_decoder(config->command_decoder() |
                                  kTokenData[token].flag);
      break;
    default:
      DCHECK(false);
      break;
  }
  return true;
}

bool GPUTestExpectationsParser::UpdateTestConfig(
    GPUTestConfig* config,
    const std::string& gpu_device_id,
    size_t line_number) {
  DCHECK(config);
  uint32_t device_id = 0;
  if (config->gpu_device_id() != 0 ||
      !base::HexStringToUInt(gpu_device_id, &device_id) ||
      device_id == 0) {
    PushErrorMessage(kErrorMessage[kErrorEntryWithGpuDeviceIdConflicts],
                     line_number);
    return false;
  }
  config->set_gpu_device_id(device_id);
  return true;
}

bool GPUTestExpectationsParser::DetectConflictsBetweenEntries() {
  bool rt = false;
  for (size_t i = 0; i < entries_.size(); ++i) {
    for (size_t j = i + 1; j < entries_.size(); ++j) {
      if (entries_[i].test_name == entries_[j].test_name &&
          entries_[i].test_config.OverlapsWith(entries_[j].test_config)) {
        PushErrorMessage(kErrorMessage[kErrorEntriesOverlap],
                         entries_[i].line_number,
                         entries_[j].line_number);
        rt = true;
      }
    }
  }
  return rt;
}

void GPUTestExpectationsParser::PushErrorMessage(
    const std::string& message, size_t line_number) {
  error_messages_.push_back(
      base::StringPrintf("Line %d : %s",
                         static_cast<int>(line_number), message.c_str()));
}

void GPUTestExpectationsParser::PushErrorMessage(
    const std::string& message,
    size_t entry1_line_number,
    size_t entry2_line_number) {
  error_messages_.push_back(
      base::StringPrintf("Line %d and %d : %s",
                         static_cast<int>(entry1_line_number),
                         static_cast<int>(entry2_line_number),
                         message.c_str()));
}

GPUTestExpectationsParser:: GPUTestExpectationEntry::GPUTestExpectationEntry()
    : test_expectation(0),
      line_number(0) {
}

}  // namespace gpu
