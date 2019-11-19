// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_USAGE_CACHE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_USAGE_CACHE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"

namespace storage {

class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemUsageCache {
 public:
  FileSystemUsageCache(bool is_incognito);
  ~FileSystemUsageCache();

  // Gets the size described in the .usage file even if dirty > 0 or
  // is_valid == false.  Returns true if the .usage file is available.
  bool GetUsage(const base::FilePath& usage_file_path, int64_t* usage);

  // Gets the dirty count in the .usage file.
  // Returns true if the .usage file is available.
  bool GetDirty(const base::FilePath& usage_file_path, uint32_t* dirty);

  // Increments or decrements the "dirty" entry in the .usage file.
  // Returns false if no .usage is available.
  bool IncrementDirty(const base::FilePath& usage_file_path);
  bool DecrementDirty(const base::FilePath& usage_file_path);

  // Notifies quota system that it needs to recalculate the usage cache of the
  // origin.  Returns false if no .usage is available.
  bool Invalidate(const base::FilePath& usage_file_path);
  bool IsValid(const base::FilePath& usage_file_path);

  // Updates the size described in the .usage file.
  bool UpdateUsage(const base::FilePath& usage_file_path, int64_t fs_usage);

  // Updates the size described in the .usage file by delta with keeping dirty
  // even if dirty > 0.
  bool AtomicUpdateUsageByDelta(const base::FilePath& usage_file_path,
                                int64_t delta);

  bool Exists(const base::FilePath& usage_file_path);
  bool Delete(const base::FilePath& usage_file_path);

  void CloseCacheFiles();

  static const base::FilePath::CharType kUsageFileName[];
  static const char kUsageFileHeader[];
  static const int kUsageFileSize;
  static const int kUsageFileHeaderSize;

 private:
  // Read the size, validity and the "dirty" entry described in the .usage file.
  // Returns less than zero if no .usage file is available.
  bool Read(const base::FilePath& usage_file_path,
            bool* is_valid,
            uint32_t* dirty,
            int64_t* usage);

  bool Write(const base::FilePath& usage_file_path,
             bool is_valid,
             int32_t dirty,
             int64_t fs_usage);

  base::File* GetFile(const base::FilePath& file_path);

  bool ReadBytes(const base::FilePath& file_path,
                 char* buffer,
                 int64_t buffer_size);
  bool WriteBytes(const base::FilePath& file_path,
                  const char* buffer,
                  int64_t buffer_size);
  bool FlushFile(const base::FilePath& file_path);
  void ScheduleCloseTimer();

  bool HasCacheFileHandle(const base::FilePath& file_path);

  // Used to verify that this is used from a single sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Used to scheduled delayed calls to CloseCacheFiles().
  base::OneShotTimer timer_;

  // Incognito usages are kept in memory and are not written to disk.
  bool is_incognito_;
  // TODO(https://crbug.com/955905): Stop using base::FilePath as the key in
  // this API as the paths are not necessarily actual on-disk locations.
  std::map<base::FilePath, std::vector<uint8_t>> incognito_usages_;

  std::map<base::FilePath, std::unique_ptr<base::File>> cache_files_;

  base::WeakPtrFactory<FileSystemUsageCache> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FileSystemUsageCache);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_USAGE_CACHE_H_
