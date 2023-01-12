// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/json_parser/in_process_json_parser.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"

namespace {
void ParseJsonOnBackgroundThread(
    scoped_refptr<base::TaskRunner> task_runner,
    const std::string& unsafe_json,
    InProcessJsonParser::SuccessCallback success_callback,
    InProcessJsonParser::ErrorCallback error_callback) {
  DCHECK(task_runner);
  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      unsafe_json, base::JSON_PARSE_RFC);
  if (value_with_error.has_value()) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(success_callback),
                                         std::move(*value_with_error)));
  } else {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  base::StringPrintf(
                                      "%s (%d:%d)",
                                      value_with_error.error().message.c_str(),
                                      value_with_error.error().line,
                                      value_with_error.error().column)));
  }
}
}  // namespace

// static
void InProcessJsonParser::Parse(const std::string& unsafe_json,
                                SuccessCallback success_callback,
                                ErrorCallback error_callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ParseJsonOnBackgroundThread,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     unsafe_json, std::move(success_callback),
                     std::move(error_callback)));
}
