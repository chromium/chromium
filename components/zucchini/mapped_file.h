// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_MAPPED_FILE_H_
#define COMPONENTS_ZUCCHINI_MAPPED_FILE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "components/zucchini/buffer_view.h"

namespace zucchini {

// A file reader wrapper.
class MappedFileReader {
 public:
  // Maps |file| to memory for reading. Also validates |file|. Errors are
  // available via HasError() and error().
  explicit MappedFileReader(base::File file);
  MappedFileReader(const MappedFileReader&) = delete;
  const MappedFileReader& operator=(const MappedFileReader&) = delete;

  const uint8_t* data() const { return buffer_.data(); }
  size_t length() const { return buffer_.length(); }
  zucchini::ConstBufferView region() const { return {data(), length()}; }

  bool HasError() { return !error_.empty() || !buffer_.IsValid(); }
  const std::string& error() { return error_; }

 private:
  std::string error_;
  base::MemoryMappedFile buffer_;
};

// A file writer wrapper. The target file is deleted on destruction unless
// Keep() is called.
class MappedFileWriter {
 public:
  // Maps |file| to memory for writing. |file_path| is needed for auto delete on
  // UNIX systems, but can be empty if auto delete is not needed. Errors are
  // available via HasError() and error().
  MappedFileWriter(const base::FilePath& file_path,
                   base::File file,
                   size_t length);
  MappedFileWriter(const MappedFileWriter&) = delete;
  const MappedFileWriter& operator=(const MappedFileWriter&) = delete;
  ~MappedFileWriter();

  uint8_t* data() { return buffer_.data(); }
  size_t length() const { return buffer_.length(); }
  zucchini::MutableBufferView region() { return {data(), length()}; }

  bool HasError() { return !error_.empty() || !buffer_.IsValid(); }
  const std::string& error() { return error_; }

  // Indicates that the file should not be deleted on destruction. Returns true
  // iff the operation succeeds.
  bool Keep();

 private:
  enum OnCloseDeleteBehavior {
    kKeep,
    kAutoDeleteOnClose,
    kManualDeleteOnClose
  };

  std::string error_;
  base::FilePath file_path_;
  base::File file_handle_;
  base::MemoryMappedFile buffer_;
  OnCloseDeleteBehavior delete_behavior_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_MAPPED_FILE_H_
