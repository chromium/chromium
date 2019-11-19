// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_stream_file.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task/lazy_task_runner.h"
#include "base/task/post_task.h"
#include "base/third_party/icu/icu_utf.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"

namespace content {

scoped_refptr<base::SequencedTaskRunner> impl_task_runner() {
  constexpr base::TaskTraits kBlockingTraits = {
      base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT};
  static base::LazySequencedTaskRunner s_sequenced_task_unner =
      LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(kBlockingTraits);
  return s_sequenced_task_unner.Get();
}

scoped_refptr<DevToolsStreamFile> DevToolsStreamFile::Create(
    DevToolsIOContext* context,
    bool binary) {
  return new DevToolsStreamFile(context, binary);
}

DevToolsStreamFile::DevToolsStreamFile(DevToolsIOContext* context, bool binary)
    : DevToolsIOContext::Stream(impl_task_runner()),
      handle_(Register(context)),
      binary_(binary),
      task_runner_(impl_task_runner()),
      had_errors_(false),
      last_read_pos_(0) {}

DevToolsStreamFile::~DevToolsStreamFile() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

bool DevToolsStreamFile::InitOnFileSequenceIfNeeded() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (had_errors_)
    return false;
  if (file_.IsValid())
    return true;
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    LOG(ERROR) << "Failed to create temporary file";
    had_errors_ = true;
    return false;
  }
  const unsigned flags = base::File::FLAG_OPEN_TRUNCATED |
                         base::File::FLAG_WRITE | base::File::FLAG_READ |
                         base::File::FLAG_DELETE_ON_CLOSE;
  file_.Initialize(temp_path, flags);
  if (!file_.IsValid()) {
    LOG(ERROR) << "Failed to open temporary file: " << temp_path.value() << ", "
               << base::File::ErrorToString(file_.error_details());
    had_errors_ = true;
    DeleteFile(temp_path, false);
    return false;
  }
  return true;
}

void DevToolsStreamFile::Read(off_t position,
                              size_t max_size,
                              ReadCallback callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DevToolsStreamFile::ReadOnFileSequence, this,
                                position, max_size, std::move(callback)));
}

void DevToolsStreamFile::Append(std::unique_ptr<std::string> data) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DevToolsStreamFile::AppendOnFileSequence, this,
                                std::move(data)));
}

void DevToolsStreamFile::ReadOnFileSequence(off_t position,
                                            size_t max_size,
                                            ReadCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Status status = StatusFailure;
  std::unique_ptr<std::string> data;
  bool base64_encoded = false;

  if (file_.IsValid()) {
    std::string buffer;
    buffer.resize(max_size);
    if (position < 0)
      position = last_read_pos_;
    int size_got = file_.ReadNoBestEffort(position, &*buffer.begin(), max_size);
    if (size_got < 0) {
      LOG(ERROR) << "Failed to read temporary file";
      had_errors_ = true;
      file_.Close();
    } else {
      // Provided client has requested sufficient large block, make their
      // life easier by not truncating in the middle of a UTF-8 character.
      if (size_got > 6 && !CBU8_IS_SINGLE(buffer[size_got - 1])) {
        base::TruncateUTF8ToByteSize(buffer, size_got, &buffer);
        size_got = buffer.size();
      } else {
        buffer.resize(size_got);
      }
      data.reset(new std::string(std::move(buffer)));
      status = size_got ? StatusSuccess : StatusEOF;
      last_read_pos_ = position + size_got;
    }
  }
  if (binary_) {
    std::string raw_data(std::move(*data));
    base::Base64Encode(raw_data, data.get());
    base64_encoded = true;
  }
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), std::move(data),
                                base64_encoded, status));
}

void DevToolsStreamFile::AppendOnFileSequence(
    std::unique_ptr<std::string> data) {
  if (!InitOnFileSequenceIfNeeded())
    return;
  int size_written = file_.WriteAtCurrentPos(&*data->begin(), data->length());
  if (size_written != static_cast<int>(data->length())) {
    LOG(ERROR) << "Failed to write temporary file";
    had_errors_ = true;
    file_.Close();
  }
}

}  // namespace content
