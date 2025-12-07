// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/streaming_blob_handle.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "sql/sqlite_result_code.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

StreamingBlobHandle::StreamingBlobHandle(
    base::PassKey<sql::Database>,
    sqlite3_blob* blob,
    base::OnceCallback<void(SqliteResultCode, const char*)> done_callback)
    : blob_handle_(blob), done_callback_(std::move(done_callback)) {
  CHECK(blob);
  CHECK(done_callback_);
}

StreamingBlobHandle::~StreamingBlobHandle() {
  Close();
}

StreamingBlobHandle::StreamingBlobHandle(StreamingBlobHandle&& other)
    : blob_handle_(std::exchange(other.blob_handle_, nullptr)),
      done_callback_(std::move(other.done_callback_)) {}

StreamingBlobHandle& StreamingBlobHandle::operator=(
    StreamingBlobHandle&& other) {
  Close();
  blob_handle_ = std::exchange(other.blob_handle_, nullptr);
  done_callback_ = std::move(other.done_callback_);
  return *this;
}

bool StreamingBlobHandle::Read(int offset, base::span<uint8_t> into) {
  CHECK(blob_handle_);
  int result = sqlite3_blob_read(blob_handle_, into.data(),
                                 base::checked_cast<int>(into.size()), offset);
  if (result != SQLITE_OK) [[unlikely]] {
    sqlite3_blob_close(blob_handle_.ExtractAsDangling());
    std::move(done_callback_)
        .Run(ToSqliteResultCode(result), "-- sqlite3_blob_read()");
    // `this` could be deleted.
    return false;
  }
  return true;
}

bool StreamingBlobHandle::Write(int offset, base::span<const uint8_t> from) {
  CHECK(blob_handle_);
  int result = sqlite3_blob_write(blob_handle_, from.data(),
                                  base::checked_cast<int>(from.size()), offset);
  if (result != SQLITE_OK) [[unlikely]] {
    sqlite3_blob_close(blob_handle_.ExtractAsDangling());
    std::move(done_callback_)
        .Run(ToSqliteResultCode(result), "-- sqlite3_blob_write()");
    // `this` could be deleted.
    return false;
  }
  return true;
}

int StreamingBlobHandle::GetSize() {
  CHECK(blob_handle_);
  return sqlite3_blob_bytes(blob_handle_);
}

void StreamingBlobHandle::Close() {
  if (blob_handle_) {
    int result = sqlite3_blob_close(blob_handle_.ExtractAsDangling());
    std::move(done_callback_)
        .Run(ToSqliteResultCode(result), "-- sqlite3_blob_close()");
  }
}

}  // namespace sql
