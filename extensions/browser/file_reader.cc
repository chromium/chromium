// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/file_reader.h"

#include <algorithm>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/browser/extension_file_task_runner.h"

FileReader::FileReader(std::vector<extensions::ExtensionResource> resources,
                       size_t max_resources_length,
                       OptionalFileSequenceTask optional_file_sequence_task,
                       DoneCallback done_callback)
    : resources_(std::move(resources)),
      max_resources_length_(max_resources_length),
      optional_file_sequence_task_(std::move(optional_file_sequence_task)),
      done_callback_(std::move(done_callback)),
      origin_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

void FileReader::Start() {
  extensions::GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FileReader::ReadFilesOnFileSequence, this));
}

FileReader::~FileReader() = default;

void FileReader::ReadFilesOnFileSequence() {
  DCHECK(
      extensions::GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  std::vector<std::unique_ptr<std::string>> data;
  data.reserve(resources_.size());
  std::optional<std::string> error;

  size_t remaining_length = max_resources_length_;
  for (const auto& resource : resources_) {
    data.push_back(std::make_unique<std::string>());
    std::string* file_data = data.back().get();
    bool success = base::ReadFileToStringWithMaxSize(
        resource.GetFilePath(), file_data, remaining_length);
    if (!success) {
      // If `file_data` is non-empty, then the file length exceeded
      // `max_resources_length_`. Otherwise, another error was encountered when
      // attempting to read the file.
      error = base::StrCat(
          {"Could not load file: '", resource.relative_path().AsUTF8Unsafe(),
           "'.", file_data->empty() ? "" : " Resource size exceeded."});

      // Clear `data` to avoid passing a partial result.
      data.clear();

      break;
    }

    remaining_length -= file_data->size();
    if (optional_file_sequence_task_) {
      optional_file_sequence_task_.Run(file_data);
    }
  }

  // Release any potentially-bound references from the file sequence task.
  optional_file_sequence_task_.Reset();

  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(done_callback_), std::move(data),
                                std::move(error)));
}
