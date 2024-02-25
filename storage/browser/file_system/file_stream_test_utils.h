// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_TEST_UTILS_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_TEST_UTILS_H_

#include <string>
#include "base/types/expected.h"
#include "net/base/net_errors.h"

namespace storage {

class FileStreamReader;
class FileStreamWriter;

// Reads up to `bytes_to_read` bytes of data from `reader`.
// Returns the bytes read or an error code.
base::expected<std::string, net::Error> ReadFromReader(FileStreamReader& reader,
                                                       size_t bytes_to_read);

// Returns the length of the file if it could be successfully retrieved,
// otherwise a net error.
int64_t GetLengthFromReader(FileStreamReader* reader);

// Writes `data` to `writer`, an initialized FileStreamWriter. Returns net::OK
// if successful, otherwise a net error.
int WriteStringToWriter(FileStreamWriter* writer, const std::string& data);

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_TEST_UTILS_H_