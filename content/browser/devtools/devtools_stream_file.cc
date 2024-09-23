// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_stream_file.h"

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/third_party/icu/icu_utf.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"

namespace content {

scoped_refptr<base::SequencedTaskRunner> impl_task_runner() {
  constexpr base::TaskTraits kBlockingTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE};
  static base::LazyThreadPoolSequencedTaskRunner s_sequenced_task_unner =
      LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(kBlockingTraits);
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
      task_runner_(impl_task_runner()) {}

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
    base::DeleteFile(temp_path);
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
  auto data = std::make_unique<std::string>();
  Status status;
  if (!file_.IsValid()) {
    status = StatusFailure;
  } else {
    status = InnerReadOnFileSequence(position, max_size, *data);
    if (status == StatusFailure) {
      had_errors_ = true;
      file_.Close();
    }
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(data), binary_, status));
}

DevToolsIOContext::Stream::Status DevToolsStreamFile::InnerReadOnFileSequence(
    off_t position,
    size_t max_size,
    std::string& buffer) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(file_.IsValid());

  if (position < 0) {
    position = last_read_pos_;
  }

  if (position >= last_written_pos_) {
    return StatusEOF;
  }

  max_size =
      std::min(max_size, static_cast<size_t>(last_written_pos_ - position));
  buffer.resize(max_size);

  std::optional<size_t> size_got =
      file_.ReadNoBestEffort(position, base::as_writable_byte_span(buffer));

  if (!size_got.has_value()) {
    LOG(ERROR) << "Failed to read temporary file";
    return StatusFailure;
  }

  // Provided client has requested sufficient large block, make their
  // life easier by not truncating in the middle of a UTF-8 character.
  if (size_got.value() > 6 && !CBU8_IS_SINGLE(buffer[size_got.value() - 1])) {
    std::string truncated;
    base::TruncateUTF8ToByteSize(buffer, size_got.value(), &truncated);
    // If the above failed, we're dealing with binary files, so
    // don't mess with them.
    if (truncated.size()) {
      buffer = std::move(truncated);
      size_got = buffer.size();
    }
  }
  buffer.resize(size_got.value());
  last_read_pos_ = position + size_got.value();
  if (binary_) {
    buffer = base::Base64Encode(buffer);
  }
  return size_got ? StatusSuccess : StatusEOF;
}

void DevToolsStreamFile::AppendOnFileSequence(
    std::unique_ptr<std::string> data) {
  if (!InitOnFileSequenceIfNeeded()) {
    return;
  }
  if (!file_.WriteAtCurrentPosAndCheck(base::as_byte_span(*data))) {
    LOG(ERROR) << "Failed to write temporary file";
    had_errors_ = true;
    file_.Close();
    return;
  }
  last_written_pos_ += data->size();
}

}  // namespace content
