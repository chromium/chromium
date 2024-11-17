// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_QUOTA_QUOTA_RESERVATION_BUFFER_H_
#define STORAGE_BROWSER_FILE_SYSTEM_QUOTA_QUOTA_RESERVATION_BUFFER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/origin.h"

namespace storage {

class QuotaReservation;
class OpenFileHandle;
class OpenFileHandleContext;
class QuotaReservationManager;

// QuotaReservationBuffer manages QuotaReservation instances.  All consumed
// quota and leaked quota by associated QuotaReservation will be staged in
// QuotaReservationBuffer, and will be committed on a modified file is closed.
// The instance keeps alive while any associated QuotaReservation or
// OpenFileHandle alive.
// This class is usually manipulated only via OpenFileHandle and
// QuotaReservation.
class QuotaReservationBuffer : public base::RefCounted<QuotaReservationBuffer> {
 public:
  QuotaReservationBuffer(
      base::WeakPtr<QuotaReservationManager> reservation_manager,
      const url::Origin& origin,
      FileSystemType type);

  QuotaReservationBuffer(const QuotaReservationBuffer&) = delete;
  QuotaReservationBuffer& operator=(const QuotaReservationBuffer&) = delete;

  scoped_refptr<QuotaReservation> CreateReservation();
  std::unique_ptr<OpenFileHandle> GetOpenFileHandle(
      QuotaReservation* reservation,
      const base::FilePath& platform_path);
  void CommitFileGrowth(int64_t quota_consumption, int64_t usage_delta);
  void DetachOpenFileHandleContext(OpenFileHandleContext* context);
  void PutReservationToBuffer(int64_t size);

  QuotaReservationManager* reservation_manager() {
    return reservation_manager_.get();
  }

  const url::Origin& origin() const { return origin_; }
  FileSystemType type() const { return type_; }

 private:
  friend class base::RefCounted<QuotaReservationBuffer>;
  virtual ~QuotaReservationBuffer();

  static bool DecrementDirtyCount(
      base::WeakPtr<QuotaReservationManager> reservation_manager,
      const url::Origin& origin,
      FileSystemType type,
      base::File::Error error,
      int64_t delta);

  // Not owned.  The destructor of OpenFileHandler should erase itself from
  // |open_files_|.
  std::map<base::FilePath, raw_ptr<OpenFileHandleContext, CtnExperimental>>
      open_files_;

  base::WeakPtr<QuotaReservationManager> reservation_manager_;

  url::Origin origin_;
  FileSystemType type_;

  int64_t reserved_quota_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_QUOTA_QUOTA_RESERVATION_BUFFER_H_
