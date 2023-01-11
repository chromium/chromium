// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/file_reader.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/browser/extension_file_task_runner.h"

FileReader::FileReader(std::vector<extensions::ExtensionResource> resources,
                       OptionalFileSequenceTask optional_file_sequence_task,
                       DoneCallback done_callback)
    : resources_(std::move(resources)),
      optional_file_sequence_task_(std::move(optional_file_sequence_task)),
      done_callback_(std::move(done_callback)),
      origin_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

void FileReader::Start() {
  extensions::GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FileReader::ReadFilesOnFileSequence, this));
}

FileReader::~FileReader() {}

void FileReader::ReadFilesOnFileSequence() {
  DCHECK(
      extensions::GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  std::vector<std::unique_ptr<std::string>> data;
  data.reserve(resources_.size());
  absl::optional<std::string> error;
  for (const auto& resource : resources_) {
    data.push_back(std::make_unique<std::string>());
    std::string* file_data = data.back().get();
    bool success = base::ReadFileToString(resource.GetFilePath(), file_data);
    if (!success) {
      error =
          base::StringPrintf("Could not load file: '%s'.",
                             resource.relative_path().AsUTF8Unsafe().c_str());
      // Clear `data` to avoid passing a partial result.
      data.clear();

      break;
    }

    if (optional_file_sequence_task_)
      optional_file_sequence_task_.Run(file_data);
  }

  // Release any potentially-bound references from the file sequence task.
  optional_file_sequence_task_.Reset();

  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(done_callback_), std::move(data),
                                std::move(error)));
}
