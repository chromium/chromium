// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_TEST_UTILS_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_TEST_UTILS_H_

#include <string>

namespace storage {

class FileStreamReader;
class FileStreamWriter;

// Reads upto |size| bytes of data from |reader|, an initialized
// FileStreamReader. The read bytes will be written to |data| and the actual
// number of bytes or the error code will be written to |result|.
void ReadFromReader(FileStreamReader* reader,
                    std::string* data,
                    size_t size,
                    int* result);

// Writes |data| to |writer|, an intialized FileStreamWriter. Returns net::OK if
// successful, otherwise a net error.
int WriteStringToWriter(FileStreamWriter* writer, const std::string& data);

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_STREAM_TEST_UTILS_H_