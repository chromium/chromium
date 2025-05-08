// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/json_file_sanitizer.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "extensions/browser/extension_file_task_runner.h"

namespace extensions {

// static
std::unique_ptr<JsonFileSanitizer> JsonFileSanitizer::CreateAndStart(
    const std::set<base::FilePath>& file_paths,
    Callback callback,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  // Note we can't use std::make_unique as we want to keep the constructor
  // private.
  std::unique_ptr<JsonFileSanitizer> sanitizer(
      new JsonFileSanitizer(std::move(callback), io_task_runner));
  sanitizer->Start(file_paths);
  return sanitizer;
}

JsonFileSanitizer::JsonFileSanitizer(
    Callback callback,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : callback_(std::move(callback)), io_task_runner_(io_task_runner) {}

JsonFileSanitizer::~JsonFileSanitizer() = default;

void JsonFileSanitizer::Start(const std::set<base::FilePath>& file_paths) {
  if (file_paths.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&JsonFileSanitizer::ReportSuccess,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  remaining_callbacks_ = file_paths.size();
  for (const base::FilePath& path : file_paths) {
    io_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&JsonFileSanitizer::ProcessFile, path),
        base::BindOnce(&JsonFileSanitizer::OnProcessedFile,
                       weak_factory_.GetWeakPtr()));
  }
}

base::expected<void, JsonFileSanitizer::Error> JsonFileSanitizer::ProcessFile(
    const base::FilePath& path) {
  std::string contents;
  bool read_success = base::ReadFileToString(path, &contents);
  bool delete_success = base::DeleteFile(path);

  if (!read_success) {
    return base::unexpected(Error::kFileReadError);
  }

  if (!delete_success) {
    return base::unexpected(Error::kFileDeleteError);
  }

  std::optional<base::Value> result = base::JSONReader::Read(contents);
  if (!result.has_value() || !result->is_dict()) {
    return base::unexpected(Error::kDecodingError);
  }

  // Reserialize the JSON and write it back to the original file.
  std::optional<std::string> json_string = base::WriteJsonWithOptions(
      *result, base::JSONWriter::OPTIONS_PRETTY_PRINT);
  if (!json_string) {
    return base::unexpected(Error::kSerializingError);
  }

  if (!base::WriteFile(path, *json_string)) {
    return base::unexpected(Error::kFileWriteError);
  }

  return base::ok();
}

void JsonFileSanitizer::OnProcessedFile(base::expected<void, Error> result) {
  if (result.has_value()) {
    if (--remaining_callbacks_ == 0) {
      ReportSuccess();
    }
  } else {
    ReportError(result.error());
  }
}

void JsonFileSanitizer::ReportSuccess() {
  std::move(callback_).Run(base::ok());
}

void JsonFileSanitizer::ReportError(Error error) {
  // Prevent any other task from reporting, we want to notify only once.
  weak_factory_.InvalidateWeakPtrs();
  std::move(callback_).Run(base::unexpected(error));
}

}  // namespace extensions
