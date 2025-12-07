// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_STREAMING_BLOB_HANDLE_H_
#define SQL_STREAMING_BLOB_HANDLE_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "sql/sqlite_result_code.h"

struct sqlite3_blob;

namespace sql {

class Database;

// Wraps a blob handle opened for streaming.
// See https://www.sqlite.org/c3ref/blob_open.html
// The handle will be closed when the instance is destroyed, or when an error
// occurs.
//
// Use `sql::Database::GetStreamingBlob()` to get an instance of this class.
// Callers are responsible for deleting this instance before attempting to
// close, poison, or raze the database. Note that even deleting an instance may
// run into an error which would cause the database's error callback to run.
class COMPONENT_EXPORT(SQL) StreamingBlobHandle {
 public:
  StreamingBlobHandle(
      base::PassKey<sql::Database>,
      sqlite3_blob* blob,
      base::OnceCallback<void(SqliteResultCode, const char*)> done_callback);
  ~StreamingBlobHandle();

  // Move is allowed to facilitate use with optional.
  StreamingBlobHandle(StreamingBlobHandle&&);
  StreamingBlobHandle& operator=(StreamingBlobHandle&&);

  StreamingBlobHandle(const StreamingBlobHandle&) = delete;
  StreamingBlobHandle& operator=(const StreamingBlobHandle&) = delete;

  // These return true for success. If they fail once, calling them again
  // will CHECK.
  [[nodiscard]] bool Read(int offset, base::span<uint8_t> into);
  [[nodiscard]] bool Write(int offset, base::span<const uint8_t> from);

  // Returns the size of the blob in bytes. This will never be non-positive.
  // After Read() or Write() has failed, calling this will CHECK.
  int GetSize();

 private:
  // Closes `blob_handle_` if it's open, invoking `done_callback_`.
  void Close();

  // This handle is owned.
  raw_ptr<sqlite3_blob> blob_handle_;

  // This callback is invoked when the blob is closed, either due to an error or
  // when `this` is destroyed normally. See `sql::Database::OnSqliteError()` for
  // documentation of the parameters.
  base::OnceCallback<void(SqliteResultCode, const char*)> done_callback_;
};

}  // namespace sql

#endif  // SQL_STREAMING_BLOB_HANDLE_H_
