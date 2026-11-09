// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "services/on_device_model/decision/laya_model.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("laya_model_main");

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  base::FilePath model_path;
  if (command_line->HasSwitch("model_path")) {
    model_path = command_line->GetSwitchValuePath("model_path");
  }
  if (model_path.empty()) {
    std::cerr << "Usage: " << argv[0] << " --model_path=<path>"
              << " [--cache_path=<path>] [--threads=<n>] [--input=<text>]\n";
    return 1;
  }

  int num_threads = 0;
  if (command_line->HasSwitch("threads")) {
    if (!base::StringToInt(command_line->GetSwitchValueASCII("threads"),
                           &num_threads) ||
        num_threads <= 0) {
      LOG(ERROR) << "Invalid --threads value.";
      return 1;
    }
  }

  base::File model_file(model_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!model_file.IsValid()) {
    LOG(ERROR) << "Failed to open model file: " << model_path << " ("
               << base::File::ErrorToString(model_file.error_details()) << ")";
    return 1;
  }

  base::File cache_file;
  if (command_line->HasSwitch("cache_path")) {
    const base::FilePath cache_path =
        command_line->GetSwitchValuePath("cache_path");
    cache_file.Initialize(cache_path, base::File::FLAG_OPEN_ALWAYS |
                                          base::File::FLAG_READ |
                                          base::File::FLAG_WRITE);
    if (!cache_file.IsValid()) {
      LOG(ERROR) << "Failed to open cache file: " << cache_path << " ("
                 << base::File::ErrorToString(cache_file.error_details())
                 << ")";
      return 1;
    }
  }

  const base::TimeTicks load_start = base::TimeTicks::Now();
  base::expected<std::unique_ptr<on_device_model::LayaModel>, std::string>
      model = on_device_model::LayaModel::Create(
          std::move(model_file), std::move(cache_file), num_threads);
  const base::TimeDelta load_time = base::TimeTicks::Now() - load_start;
  if (!model.has_value()) {
    LOG(ERROR) << "Failed to initialize LayaModel from: " << model_path << " ("
               << model.error() << ")";
    return 1;
  }

  std::cout << "Loaded model: " << model_path.AsUTF8Unsafe() << " ("
            << load_time.InMillisecondsF() << " ms)\n";

  // Uses a fixed sentiment-analysis choice question by default.
  // TODO(crbug.com/565849508): Support custom schemas via command-line flags
  // and cover multiple question types in the default query.
  auto question = on_device_model::mojom::DecisionQuestion::New();
  question->id = "sentiment";
  question->prompt = "What is the sentiment of this review?";
  question->type = on_device_model::mojom::DecisionQuestionType::kChoice;
  question->options.push_back(on_device_model::mojom::DecisionOption::New(
      "positive", "The review expresses satisfaction or praise"));
  question->options.push_back(on_device_model::mojom::DecisionOption::New(
      "negative", "The review expresses disappointment or criticism"));

  const std::string prompt = question->prompt;
  std::vector<std::string> option_labels;
  for (const auto& opt : question->options) {
    option_labels.push_back(opt->label);
  }

  auto query = on_device_model::mojom::DecisionQuery::New();
  query->questions.push_back(std::move(question));

  base::expected<on_device_model::LayaModel::Session, std::string> session =
      (*model)->CreateSession(std::move(query));
  if (!session.has_value()) {
    LOG(ERROR) << "Failed to create LayaModel session: " << session.error();
    return 1;
  }

  std::vector<std::string> inputs;
  if (command_line->HasSwitch("input")) {
    inputs.push_back(command_line->GetSwitchValueASCII("input"));
  } else {
    inputs.push_back(
        "This movie was an absolute masterpiece, I loved every minute of it!");
    inputs.push_back(
        "Terrible service, broken product, complete waste of money.");
  }

  std::cout << "\nQuestion: " << prompt << "\n";
  for (const std::string& input : inputs) {
    const base::TimeTicks exec_start = base::TimeTicks::Now();
    base::expected<std::vector<std::vector<float>>, std::string> results =
        session->Execute(input);
    const base::TimeDelta exec_time = base::TimeTicks::Now() - exec_start;

    if (!results.has_value() || results->empty() ||
        (*results)[0].size() != option_labels.size()) {
      LOG(ERROR) << "Execution failed for input: " << input << " ("
                 << (results.has_value() ? "unexpected result shape"
                                         : results.error())
                 << ")";
      return 1;
    }

    std::cout << "\nInput: \"" << input << "\" (" << exec_time.InMillisecondsF()
              << " ms)\n";
    for (size_t i = 0; i < option_labels.size(); ++i) {
      std::cout << "  " << option_labels[i] << ": " << (*results)[0][i] << "\n";
    }
  }

  return 0;
}
