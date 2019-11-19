// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/json_parser/in_process_json_parser.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"

namespace {
void ParseJsonOnBackgroundThread(
    scoped_refptr<base::TaskRunner> task_runner,
    const std::string& unsafe_json,
    InProcessJsonParser::SuccessCallback success_callback,
    InProcessJsonParser::ErrorCallback error_callback) {
  DCHECK(task_runner);
  base::JSONReader::ValueWithError value_with_error =
      base::JSONReader::ReadAndReturnValueWithError(unsafe_json,
                                                    base::JSON_PARSE_RFC);
  if (value_with_error.value) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(success_callback),
                                         std::move(*value_with_error.value)));
  } else {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(error_callback),
            base::StringPrintf(
                "%s (%d:%d)", value_with_error.error_message.c_str(),
                value_with_error.error_line, value_with_error.error_column)));
  }
}
}  // namespace

// static
void InProcessJsonParser::Parse(const std::string& unsafe_json,
                                SuccessCallback success_callback,
                                ErrorCallback error_callback) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ParseJsonOnBackgroundThread,
                     base::ThreadTaskRunnerHandle::Get(), unsafe_json,
                     std::move(success_callback), std::move(error_callback)));
}
