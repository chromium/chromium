// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/memory_file_stream_reader.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace storage {

MemoryFileStreamReader::MemoryFileStreamReader(
    scoped_refptr<base::TaskRunner> task_runner,
    base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> memory_file_util,
    const base::FilePath& file_path,
    int64_t initial_offset,
    const base::Time& expected_modification_time)
    : memory_file_util_(std::move(memory_file_util)),
      task_runner_(std::move(task_runner)),
      file_path_(file_path),
      expected_modification_time_(expected_modification_time),
      offset_(initial_offset) {
  DCHECK(memory_file_util_.MaybeValid());
}

MemoryFileStreamReader::~MemoryFileStreamReader() = default;

int MemoryFileStreamReader::Read(net::IOBuffer* buf,
                                 int buf_len,
                                 net::CompletionOnceCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> util,
             const base::FilePath& path, base::Time expected_modification_time,
             int64_t offset, scoped_refptr<net::IOBuffer> buf,
             int buf_len) -> int {
            if (!util)
              return net::ERR_FILE_NOT_FOUND;
            base::File::Info file_info;
            if (util->GetFileInfo(path, &file_info) != base::File::FILE_OK)
              return net::ERR_FILE_NOT_FOUND;

            if (!FileStreamReader::VerifySnapshotTime(
                    expected_modification_time, file_info)) {
              return net::ERR_UPLOAD_FILE_CHANGED;
            }

            return util->ReadFile(path, offset, std::move(buf), buf_len);
          },
          memory_file_util_, file_path_, expected_modification_time_, offset_,
          base::WrapRefCounted(buf), buf_len),
      base::BindOnce(&MemoryFileStreamReader::OnReadCompleted,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  return net::ERR_IO_PENDING;
}

void MemoryFileStreamReader::OnReadCompleted(
    net::CompletionOnceCallback callback,
    int result) {
  if (result > 0)
    offset_ += result;

  std::move(callback).Run(result);
}

int64_t MemoryFileStreamReader::GetLength(
    net::Int64CompletionOnceCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> util,
             const base::FilePath& path,
             base::Time expected_modification_time) -> int64_t {
            if (!util)
              return net::ERR_FILE_NOT_FOUND;
            base::File::Info file_info;
            if (util->GetFileInfo(path, &file_info) != base::File::FILE_OK) {
              return net::ERR_FILE_NOT_FOUND;
            }

            if (!FileStreamReader::VerifySnapshotTime(
                    expected_modification_time, file_info)) {
              return net::ERR_UPLOAD_FILE_CHANGED;
            }

            return file_info.size;
          },
          memory_file_util_, file_path_, expected_modification_time_),
      // |callback| is not directly used to make sure that it is not called if
      // stream is deleted while this function is in flight.
      base::BindOnce(&MemoryFileStreamReader::OnGetLengthCompleted,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  return net::ERR_IO_PENDING;
}

void MemoryFileStreamReader::OnGetLengthCompleted(
    net::Int64CompletionOnceCallback callback,
    int64_t result) {
  std::move(callback).Run(result);
}

}  // namespace storage
