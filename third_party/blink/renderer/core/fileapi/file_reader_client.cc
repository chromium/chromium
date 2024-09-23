// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"

#include "third_party/blink/renderer/core/fileapi/file_read_type.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

FileErrorCode FileReaderAccumulator::DidStartLoading(uint64_t total_bytes) {
  bytes_loaded_ = 0;
  raw_data_ = ArrayBufferContents(static_cast<unsigned>(total_bytes), 1,
                                  ArrayBufferContents::kNotShared,
                                  ArrayBufferContents::kDontInitialize);
  if (!raw_data_.IsValid()) {
    return FileErrorCode::kNotReadableErr;
  }
  return DidStartLoading();
}

FileErrorCode FileReaderAccumulator::DidReceiveData(
    base::span<const uint8_t> data) {
  // Fill out the buffer
  if (bytes_loaded_ + data.size() > raw_data_.DataLength()) {
    raw_data_.Reset();
    bytes_loaded_ = 0;
    return FileErrorCode::kNotReadableErr;
  }
  raw_data_.ByteSpan()
      .subspan(base::checked_cast<size_t>(bytes_loaded_))
      .copy_prefix_from(data);
  bytes_loaded_ += data.size();
  return DidReceiveData();
}

void FileReaderAccumulator::DidFinishLoading() {
  DCHECK_EQ(bytes_loaded_, raw_data_.DataLength());
  CHECK(raw_data_.IsValid());
  DidFinishLoading(FileReaderData(std::move(raw_data_)));
}

void FileReaderAccumulator::DidFail(FileErrorCode) {
  bytes_loaded_ = 0;
  raw_data_.Reset();
}

std::pair<FileErrorCode, FileReaderData> SyncedFileReaderAccumulator::Load(
    scoped_refptr<BlobDataHandle> handle,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto* client = MakeGarbageCollected<SyncedFileReaderAccumulator>();
  auto* file_reader =
      MakeGarbageCollected<FileReaderLoader>(client, std::move(task_runner));

  file_reader->StartSync(std::move(handle));
  return {client->error_code_, std::move(client->stored_)};
}

void SyncedFileReaderAccumulator::DidFail(FileErrorCode error_code) {
  error_code_ = error_code;
}
void SyncedFileReaderAccumulator::DidFinishLoading(FileReaderData obj) {
  stored_ = std::move(obj);
}

}  // namespace blink
