// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/ort_session_options.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/test_base_ort.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/webnn_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_session_options_config_keys.h"

namespace webnn::ort {

namespace {

std::string GetSessionConfigEntry(const OrtApi* ort_api,
                                  const OrtSessionOptions* session_options,
                                  const char* key) {
  size_t value_size = 0;
  EXPECT_EQ(ort_api->GetSessionConfigEntry(session_options, key, nullptr,
                                           &value_size),
            nullptr);
  EXPECT_GT(value_size, 0u);
  if (value_size == 0) {
    return {};
  }

  std::string value(value_size, '\0');
  EXPECT_EQ(ort_api->GetSessionConfigEntry(session_options, key, value.data(),
                                           &value_size),
            nullptr);
  EXPECT_GT(value_size, 0u);
  if (value_size == 0) {
    return {};
  }
  value.resize(value_size - 1);
  return value;
}

class WebNNOrtSessionOptionsTest : public TestBaseOrt {};

TEST_F(WebNNOrtSessionOptionsTest, AppliesEntries) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebNNOrtSessionConfigEntriesForTesting,
      "key1,value1,key2,value2");

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  ApplySessionConfigEntriesFromCommandLine(
      session_options.get(), *scoped_command_line.GetProcessCommandLine());

  EXPECT_EQ(GetSessionConfigEntry(ort_api, session_options.get(), "key1"),
            "value1");
  EXPECT_EQ(GetSessionConfigEntry(ort_api, session_options.get(), "key2"),
            "value2");
}

TEST_F(WebNNOrtSessionOptionsTest, EmptySwitchIsIgnored) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kWebNNOrtSessionConfigEntriesForTesting);

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  ApplySessionConfigEntriesFromCommandLine(
      session_options.get(), *scoped_command_line.GetProcessCommandLine());
}

TEST_F(WebNNOrtSessionOptionsTest, EmptyValueIsAccepted) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebNNOrtSessionConfigEntriesForTesting, "key,");

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  ApplySessionConfigEntriesFromCommandLine(
      session_options.get(), *scoped_command_line.GetProcessCommandLine());

  EXPECT_EQ(GetSessionConfigEntry(ort_api, session_options.get(), "key"), "");
}

TEST_F(WebNNOrtSessionOptionsTest, MissingValueFailsFast) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebNNOrtSessionConfigEntriesForTesting, "key1,value1,key2");

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  EXPECT_DEATH(
      ApplySessionConfigEntriesFromCommandLine(
          session_options.get(), *scoped_command_line.GetProcessCommandLine()),
      "webnn-ort-session-config-entries-for-testing.*received.*key2");
}

TEST_F(WebNNOrtSessionOptionsTest, EmptyKeyFailsFast) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebNNOrtSessionConfigEntriesForTesting, ",value");

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  EXPECT_DEATH(
      ApplySessionConfigEntriesFromCommandLine(
          session_options.get(), *scoped_command_line.GetProcessCommandLine()),
      "webnn-ort-session-config-entries-for-testing.*non-empty key.*received.*"
      "value");
}

TEST_F(WebNNOrtSessionOptionsTest, WhitespaceOnlyKeyFailsFast) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebNNOrtSessionConfigEntriesForTesting, "   ,value");

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  EXPECT_DEATH(
      ApplySessionConfigEntriesFromCommandLine(
          session_options.get(), *scoped_command_line.GetProcessCommandLine()),
      "requires a non-empty key.*   ,value");
}

TEST_F(WebNNOrtSessionOptionsTest, ExistingEntryFailsFastWithContext) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebNNOrtSessionConfigEntriesForTesting,
      std::string(kOrtSessionOptionsDisableCPUEPFallback) + ",1");

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);
  ASSERT_EQ(
      ort_api->AddSessionConfigEntry(
          session_options.get(), kOrtSessionOptionsDisableCPUEPFallback, "0"),
      nullptr);

  EXPECT_DEATH(
      ApplySessionConfigEntriesFromCommandLine(
          session_options.get(), *scoped_command_line.GetProcessCommandLine()),
      "webnn-ort-session-config-entries-for-testing.*"
      "session.disable_cpu_ep_fallback.*1");
}

TEST_F(WebNNOrtSessionOptionsTest, DuplicateInputEntryFailsFastWithContext) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebNNOrtSessionConfigEntriesForTesting,
      "duplicate,first,duplicate,second");

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  EXPECT_DEATH(
      ApplySessionConfigEntriesFromCommandLine(
          session_options.get(), *scoped_command_line.GetProcessCommandLine()),
      "webnn-ort-session-config-entries-for-testing.*duplicate.*second");
}

TEST_F(WebNNOrtSessionOptionsTest, OrtRejectedEntryFailsFastWithContext) {
  base::test::ScopedCommandLine scoped_command_line;
  const std::string key(1025, 'k');
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kWebNNOrtSessionConfigEntriesForTesting, key + ",value");

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  EXPECT_DEATH(
      ApplySessionConfigEntriesFromCommandLine(
          session_options.get(), *scoped_command_line.GetProcessCommandLine()),
      "Failed to apply --webnn-ort-session-config-entries-for-testing entry "
      "\"k+,value\" from \"k+,value\": .+");
}

TEST_F(WebNNOrtSessionOptionsTest, DefaultLoggingKeepsOpenVINOConfigEntries) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->RemoveSwitch(
      switches::kWebNNOrtLoggingLevel);
  const SessionConfigEntry& load_config =
      kKnownEPs.at(kOpenVINOExecutionProvider).config_entries[0];

  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  ScopedOrtSessionOptions session_options;
  ASSERT_EQ(ort_api->CreateSessionOptions(
                ScopedOrtSessionOptions::Receiver(session_options).get()),
            nullptr);

  ApplyEpConfigEntries(session_options.get(), kOpenVINOExecutionProvider);

  EXPECT_EQ(GetSessionConfigEntry(ort_api, session_options.get(),
                                  load_config.key.c_str()),
            load_config.value);
}

TEST_F(WebNNOrtSessionOptionsTest,
       InfoOrVerboseLoggingAddsOpenVINONpuLogLevel) {
  const SessionConfigEntry& load_config =
      kKnownEPs.at(kOpenVINOExecutionProvider).config_entries[0];
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  for (const char* logging_level : {"INFO", "VERBOSE"}) {
    SCOPED_TRACE(logging_level);
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->RemoveSwitch(
        switches::kWebNNOrtLoggingLevel);
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kWebNNOrtLoggingLevel, logging_level);

    ScopedOrtSessionOptions session_options;
    ASSERT_EQ(ort_api->CreateSessionOptions(
                  ScopedOrtSessionOptions::Receiver(session_options).get()),
              nullptr);

    ApplyEpConfigEntries(session_options.get(), kOpenVINOExecutionProvider);

    base::DictValue applied_config =
        base::test::ParseJsonDict(GetSessionConfigEntry(
            ort_api, session_options.get(), load_config.key.c_str()));
    std::optional<base::Value> npu_log_level =
        applied_config.ExtractByDottedPath("NPU.LOG_LEVEL");
    ASSERT_TRUE(npu_log_level);
    EXPECT_EQ(*npu_log_level, base::Value("LOG_INFO"));
    base::DictValue default_config =
        base::test::ParseJsonDict(load_config.value);
    EXPECT_EQ(applied_config, default_config);
  }
}

TEST_F(WebNNOrtSessionOptionsTest,
       InfoOrVerboseLoggingKeepsNonOpenVINOConfigEntries) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();

  for (const char* logging_level : {"INFO", "VERBOSE"}) {
    SCOPED_TRACE(logging_level);
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->RemoveSwitch(
        switches::kWebNNOrtLoggingLevel);
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kWebNNOrtLoggingLevel, logging_level);

    ScopedOrtSessionOptions session_options;
    ASSERT_EQ(ort_api->CreateSessionOptions(
                  ScopedOrtSessionOptions::Receiver(session_options).get()),
              nullptr);

    ApplyEpConfigEntries(session_options.get(), kWebGpuExecutionProvider);

    for (const auto& [key, value] :
         kKnownEPs.at(kWebGpuExecutionProvider).config_entries) {
      EXPECT_EQ(
          GetSessionConfigEntry(ort_api, session_options.get(), key.c_str()),
          value);
    }
  }
}

}  // namespace

}  // namespace webnn::ort
