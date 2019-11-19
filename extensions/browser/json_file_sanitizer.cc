// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/json_file_sanitizer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/task_runner_util.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions {

namespace {

// Reads the file in |path| and then deletes it.
// Returns a tuple containing: the file content, whether the read was
// successful, whether the delete was successful.
std::tuple<std::string, bool, bool> ReadAndDeleteTextFile(
    const base::FilePath& path) {
  std::string contents;
  bool read_success = base::ReadFileToString(path, &contents);
  bool delete_success = base::DeleteFile(path, /*recursive=*/false);
  return std::make_tuple(contents, read_success, delete_success);
}

int WriteStringToFile(const std::string& contents,
                      const base::FilePath& file_path) {
  int size = static_cast<int>(contents.length());
  return base::WriteFile(file_path, contents.data(), size);
}

}  // namespace

// static
std::unique_ptr<JsonFileSanitizer> JsonFileSanitizer::CreateAndStart(
    data_decoder::DataDecoder* decoder,
    const std::set<base::FilePath>& file_paths,
    Callback callback) {
  // Note we can't use std::make_unique as we want to keep the constructor
  // private.
  std::unique_ptr<JsonFileSanitizer> sanitizer(
      new JsonFileSanitizer(file_paths, std::move(callback)));
  sanitizer->Start(decoder);
  return sanitizer;
}

JsonFileSanitizer::JsonFileSanitizer(const std::set<base::FilePath>& file_paths,
                                     Callback callback)
    : file_paths_(file_paths), callback_(std::move(callback)) {}

JsonFileSanitizer::~JsonFileSanitizer() = default;

void JsonFileSanitizer::Start(data_decoder::DataDecoder* decoder) {
  if (file_paths_.empty()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&JsonFileSanitizer::ReportSuccess,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  decoder->GetService()->BindJsonParser(
      json_parser_.BindNewPipeAndPassReceiver());

  for (const base::FilePath& path : file_paths_) {
    base::PostTaskAndReplyWithResult(
        extensions::GetExtensionFileTaskRunner().get(), FROM_HERE,
        base::BindOnce(&ReadAndDeleteTextFile, path),
        base::BindOnce(&JsonFileSanitizer::JsonFileRead,
                       weak_factory_.GetWeakPtr(), path));
  }
}

void JsonFileSanitizer::JsonFileRead(
    const base::FilePath& file_path,
    std::tuple<std::string, bool, bool> read_and_delete_result) {
  if (!std::get<1>(read_and_delete_result)) {
    ReportError(Status::kFileReadError, std::string());
    return;
  }
  if (!std::get<2>(read_and_delete_result)) {
    ReportError(Status::kFileDeleteError, std::string());
    return;
  }
  json_parser_->Parse(std::get<0>(read_and_delete_result),
                      base::BindOnce(&JsonFileSanitizer::JsonParsingDone,
                                     weak_factory_.GetWeakPtr(), file_path));
}

void JsonFileSanitizer::JsonParsingDone(
    const base::FilePath& file_path,
    base::Optional<base::Value> json_value,
    const base::Optional<std::string>& error) {
  if (!json_value || !json_value->is_dict()) {
    ReportError(Status::kDecodingError, error ? *error : std::string());
    return;
  }

  // Reserialize the JSON and write it back to the original file.
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(*json_value)) {
    ReportError(Status::kSerializingError, std::string());
    return;
  }

  int size = static_cast<int>(json_string.length());
  base::PostTaskAndReplyWithResult(
      extensions::GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::BindOnce(&WriteStringToFile, std::move(json_string), file_path),
      base::BindOnce(&JsonFileSanitizer::JsonFileWritten,
                     weak_factory_.GetWeakPtr(), file_path, size));
}

void JsonFileSanitizer::JsonFileWritten(const base::FilePath& file_path,
                                        int expected_size,
                                        int actual_size) {
  if (expected_size != actual_size) {
    ReportError(Status::kFileWriteError, std::string());
    return;
  }
  // We have finished with this JSON file.
  size_t removed_count = file_paths_.erase(file_path);
  DCHECK_EQ(1U, removed_count);

  if (file_paths_.empty()) {
    // This was the last path, we are done.
    ReportSuccess();
  }
}

void JsonFileSanitizer::ReportSuccess() {
  std::move(callback_).Run(Status::kSuccess, std::string());
}

void JsonFileSanitizer::ReportError(Status status, const std::string& error) {
  // Prevent any other task from reporting, we want to notify only once.
  weak_factory_.InvalidateWeakPtrs();
  std::move(callback_).Run(status, error);
}

}  // namespace extensions
