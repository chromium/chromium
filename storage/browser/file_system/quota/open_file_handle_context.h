// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_QUOTA_OPEN_FILE_HANDLE_CONTEXT_H_
#define STORAGE_BROWSER_FILE_SYSTEM_QUOTA_OPEN_FILE_HANDLE_CONTEXT_H_

#include <stdint.h>

#include <map>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/gurl.h"

namespace storage {

class QuotaReservationBuffer;

// This class represents a underlying file of a managed FileSystem file.
// The instance keeps alive while at least one consumer keeps an open file
// handle.
// This class is usually manipulated only via OpenFileHandle.
class OpenFileHandleContext : public base::RefCounted<OpenFileHandleContext> {
 public:
  OpenFileHandleContext(const base::FilePath& platform_path,
                        QuotaReservationBuffer* reservation_buffer);

  // Updates the max written offset and returns the amount of growth.
  int64_t UpdateMaxWrittenOffset(int64_t offset);

  void AddAppendModeWriteAmount(int64_t amount);

  const base::FilePath& platform_path() const { return platform_path_; }

  int64_t GetEstimatedFileSize() const;
  int64_t GetMaxWrittenOffset() const;

 private:
  friend class base::RefCounted<OpenFileHandleContext>;
  virtual ~OpenFileHandleContext();

  int64_t initial_file_size_;
  int64_t maximum_written_offset_;
  int64_t append_mode_write_amount_;
  base::FilePath platform_path_;

  scoped_refptr<QuotaReservationBuffer> reservation_buffer_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(OpenFileHandleContext);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_QUOTA_OPEN_FILE_HANDLE_CONTEXT_H_
