// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/trace_startup_config.h"

#include <algorithm>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "components/tracing/common/tracing_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"

namespace tracing {

namespace {

const char kTraceConfig[] =
    "{"
    "\"enable_argument_filter\":true,"
    "\"enable_package_name_filter\":false,"
    "\"enable_systrace\":true,"
    "\"excluded_categories\":[\"excluded\",\"exc_pattern*\"],"
    "\"included_categories\":[\"included\","
    "\"inc_pattern*\","
    "\"disabled-by-default-cc\"],"
    "\"record_mode\":\"record-continuously\""
    "}";

std::string GetTraceConfigFileContent(std::string trace_config,
                                      std::string startup_duration,
                                      std::string result_file) {
  std::string content = "{";
  if (!trace_config.empty()) {
    content += "\"trace_config\":" + trace_config;
  }

  if (!startup_duration.empty()) {
    if (content != "{") {
      content += ",";
    }
    content += "\"startup_duration\":" + startup_duration;
  }

  if (!result_file.empty()) {
    if (content != "{") {
      content += ",";
    }
    content += "\"result_file\":\"" + result_file + "\"";
  }

  content += "}";
  return content;
}

}  // namespace

class TraceStartupConfigTest : public ::testing::Test {
 protected:
  void Initialize() {
    startup_config_ = base::WrapUnique(new TraceStartupConfig());
  }

  std::unique_ptr<TraceStartupConfig> startup_config_;
};

TEST_F(TraceStartupConfigTest, TraceStartupEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTraceStartup);
  Initialize();
  EXPECT_TRUE(startup_config_->IsEnabled());
}

TEST_F(TraceStartupConfigTest, TraceStartupConfigNotEnabled) {
  Initialize();
  EXPECT_FALSE(startup_config_->IsEnabled());
}

TEST_F(TraceStartupConfigTest, TraceStartupConfigEnabledWithoutPath) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kTraceConfigFile);

  Initialize();
  ASSERT_TRUE(startup_config_->IsEnabled());
  auto config = startup_config_->GetPerfettoConfig();
  ASSERT_LE(2, config.data_sources_size());
  EXPECT_EQ("track_event", config.data_sources()[0].config().name());
  EXPECT_EQ(
      base::trace_event::TraceConfig().ToPerfettoTrackEventConfigRaw(false),
      config.data_sources()[0].config().track_event_config_raw());
  EXPECT_EQ(5000U, config.duration_ms());
  EXPECT_TRUE(startup_config_->GetResultFile().empty());
}

TEST_F(TraceStartupConfigTest, TraceStartupConfigEnabledWithInvalidPath) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile,
      base::FilePath(FILE_PATH_LITERAL("invalid-trace-config-file-path")));

  Initialize();
  EXPECT_FALSE(startup_config_->IsEnabled());
}

TEST_F(TraceStartupConfigTest, ValidContent) {
  std::string content =
      GetTraceConfigFileContent(kTraceConfig, "10", "trace_result_file.log");

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_TRUE(base::WriteFile(trace_config_file, content));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  Initialize();
  ASSERT_TRUE(startup_config_->IsEnabled());

  auto config = startup_config_->GetPerfettoConfig();
  ASSERT_LE(2, config.data_sources_size());
  EXPECT_EQ("track_event", config.data_sources()[0].config().name());
  EXPECT_EQ(base::trace_event::TraceConfig(kTraceConfig)
                .ToPerfettoTrackEventConfigRaw(false),
            config.data_sources()[0].config().track_event_config_raw());
  EXPECT_EQ(10000U, config.duration_ms());
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("trace_result_file.log")),
            startup_config_->GetResultFile());
}

TEST_F(TraceStartupConfigTest, ValidContentWithOnlyTraceConfig) {
  std::string content = GetTraceConfigFileContent(kTraceConfig, "", "");

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_TRUE(base::WriteFile(trace_config_file, content));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  Initialize();
  ASSERT_TRUE(startup_config_->IsEnabled());

  auto config = startup_config_->GetPerfettoConfig();
  ASSERT_LE(2, config.data_sources_size());
  EXPECT_EQ("track_event", config.data_sources()[0].config().name());
  EXPECT_EQ(base::trace_event::TraceConfig(kTraceConfig)
                .ToPerfettoTrackEventConfigRaw(false),
            config.data_sources()[0].config().track_event_config_raw());
  EXPECT_FALSE(config.has_duration_ms());
  EXPECT_TRUE(startup_config_->GetResultFile().empty());
}

TEST_F(TraceStartupConfigTest, ContentWithAbsoluteResultFilePath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath result_file_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("trace_result_file.log"));
  ASSERT_TRUE(result_file_path.IsAbsolute());

  std::string result_file_path_str = result_file_path.AsUTF8Unsafe();
  auto it = base::ranges::find(result_file_path_str, '\\');
  while (it != result_file_path_str.end()) {
    auto it2 = result_file_path_str.insert(it, '\\');
    it = std::find(it2 + 2, result_file_path_str.end(), '\\');
  }
  std::string content =
      GetTraceConfigFileContent(kTraceConfig, "10", result_file_path_str);

  base::FilePath trace_config_file;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_TRUE(base::WriteFile(trace_config_file, content));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  Initialize();
  ASSERT_TRUE(startup_config_->IsEnabled());
  EXPECT_EQ(result_file_path, startup_config_->GetResultFile());
}

TEST_F(TraceStartupConfigTest, ContentWithNegtiveDuration) {
  std::string content = GetTraceConfigFileContent(kTraceConfig, "-1", "");

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_TRUE(base::WriteFile(trace_config_file, content));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  Initialize();
  ASSERT_TRUE(startup_config_->IsEnabled());

  auto config = startup_config_->GetPerfettoConfig();
  ASSERT_LE(2, config.data_sources_size());
  EXPECT_EQ("track_event", config.data_sources()[0].config().name());
  EXPECT_EQ(base::trace_event::TraceConfig(kTraceConfig)
                .ToPerfettoTrackEventConfigRaw(false),
            config.data_sources()[0].config().track_event_config_raw());
  EXPECT_FALSE(config.has_duration_ms());
  EXPECT_TRUE(startup_config_->GetResultFile().empty());
}

TEST_F(TraceStartupConfigTest, ContentWithoutTraceConfig) {
  std::string content =
      GetTraceConfigFileContent("", "10", "trace_result_file.log");

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_TRUE(base::WriteFile(trace_config_file, content));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  Initialize();
  EXPECT_FALSE(startup_config_->IsEnabled());
}

TEST_F(TraceStartupConfigTest, InvalidContent) {
  std::string content = "invalid trace config file content";

  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  ASSERT_TRUE(base::WriteFile(trace_config_file, content));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  Initialize();
  EXPECT_FALSE(startup_config_->IsEnabled());
}

TEST_F(TraceStartupConfigTest, EmptyContent) {
  base::FilePath trace_config_file;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &trace_config_file));
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kTraceConfigFile, trace_config_file);

  Initialize();
  EXPECT_FALSE(startup_config_->IsEnabled());
}

TEST_F(TraceStartupConfigTest, TraceStartupDisabledSystemOwner) {
  // Set the owner to 'system' is not sufficient to setup startup tracing.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTraceStartupOwner, "system");
  Initialize();
  EXPECT_FALSE(startup_config_->IsEnabled());
}

TEST_F(TraceStartupConfigTest, TraceStartupEnabledSystemOwner) {
  // With owner and --trace-startup TraceStartupConfig should be enabled with
  // the owner being the system.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTraceStartupOwner, "system");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kTraceStartup);
  Initialize();
  EXPECT_TRUE(startup_config_->IsEnabled());
  EXPECT_EQ(TraceStartupConfig::SessionOwner::kSystemTracing,
            startup_config_->GetSessionOwner());
}

}  // namespace tracing
