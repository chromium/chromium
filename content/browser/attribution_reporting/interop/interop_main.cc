// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/attribution_reporting/interop/parser.h"
#include "content/browser/attribution_reporting/interop/runner.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_content_client_initializer.h"
#include "mojo/core/embedder/embedder.h"

namespace {

const content::aggregation_service::TestHpkeKey kHpkeKey;

class Env : public content::ContentTestSuiteBase {
 public:
  Env(int argc, char** argv) : ContentTestSuiteBase(argc, argv) {
    Initialize();
    content::ForceInProcessNetworkService();
    // This initialization depends on the call to `Initialize()`, so we use a
    // `unique_ptr` to defer initialization instead of storing the field
    // directly.
    test_content_initializer_ =
        std::make_unique<content::TestContentClientInitializer>();

    mojo::core::Init();
  }

  ~Env() override = default;

 private:
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_initializer_;
};

std::optional<base::Value::Dict> ReadDictFromFile(
    const base::CommandLine& cmd_line,
    const char* flag) {
  if (!cmd_line.HasSwitch(flag)) {
    std::cerr << "must specify --" << flag << std::endl;
    return std::nullopt;
  }

  std::string json;
  if (!base::ReadFileToString(cmd_line.GetSwitchValuePath(flag), &json)) {
    std::cerr << "failed to read " << flag << " path" << std::endl;
    return std::nullopt;
  }

  auto result = base::JSONReader::ReadAndReturnValueWithError(json);
  if (!result.has_value()) {
    std::cerr << "failed to parse " << flag
              << " as JSON: " << result.error().message << std::endl;
    return std::nullopt;
  }

  base::Value::Dict* dict = result->GetIfDict();
  if (!dict) {
    std::cerr << flag << " JSON must be a dictionary" << std::endl;
    return std::nullopt;
  }

  return std::move(*dict);
}

}  // namespace

// See //content/test/data/attribution_reporting/interop/README.md for details
// on the input format.
int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();

  auto input = ReadDictFromFile(cmd_line, "input");
  if (!input.has_value()) {
    return 1;
  }

  auto default_config_dict = ReadDictFromFile(cmd_line, "default_config");
  if (!default_config_dict.has_value()) {
    return 1;
  }

  auto default_config =
      content::ParseAttributionInteropConfig(*std::move(default_config_dict));
  if (!default_config.has_value()) {
    std::cerr << "failed to parse default config: " << default_config.error()
              << std::endl;
    return 1;
  }

  auto run =
      content::AttributionInteropRun::Parse(*std::move(input), *default_config);
  if (!run.has_value()) {
    std::cerr << "failed to parse input: " << run.error() << std::endl;
    return 1;
  }

  Env env(argc, argv);

  auto output =
      content::RunAttributionInteropSimulation(*std::move(run), kHpkeKey);
  if (!output.has_value()) {
    std::cerr << output.error() << std::endl;
    return 1;
  }

  std::cout << output->ToJson() << std::endl;
  return 0;
}
